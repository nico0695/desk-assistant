#include "bridge_http.h"

#include <limits.h>
#include <string.h>

#include "bridge_clock.h"
#include "bridge_http_common.h"
#include "bridge_tls.h"
#include "esp_http_client.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "sdkconfig.h"

#if CONFIG_BRIDGE_DIAGNOSTIC_PROBE
#include "memory_report.h"
#endif

static SemaphoreHandle_t finite_gate;
static SemaphoreHandle_t handshake_gate;

esp_err_t bridge_http_init(void)
{
    if (finite_gate != NULL && handshake_gate != NULL) {
        return ESP_OK;
    }
    finite_gate = xSemaphoreCreateMutex();
    if (finite_gate == NULL) {
        return ESP_ERR_NO_MEM;
    }
    handshake_gate = xSemaphoreCreateMutex();
    if (handshake_gate == NULL) {
        vSemaphoreDelete(finite_gate);
        finite_gate = NULL;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

bool bridge_http_handshake_acquire(uint32_t wait_ms)
{
    return handshake_gate != NULL && xSemaphoreTake(handshake_gate, pdMS_TO_TICKS(wait_ms)) == pdTRUE;
}

void bridge_http_handshake_release(void)
{
    xSemaphoreGive(handshake_gate);
}


static bridge_http_result_t failure_result(const bridge_tls_t *session)
{
    if (session->wire_limited) {
        return BRIDGE_HTTP_TOO_LARGE;
    }
    return session->timed_out || esp_timer_get_time() >= session->deadline_us ? BRIDGE_HTTP_TIMEOUT
                                                                               : BRIDGE_HTTP_TRANSPORT_ERROR;
}

/* Reads the whole body into the caller's buffer; one byte past capacity proves the body too large. */
static bridge_http_result_t read_body(bridge_tls_t *session, char *body, size_t capacity, size_t *length)
{
    for (;;) {
        if (bridge_tls_budget(session, -1) <= 0) {
            return BRIDGE_HTTP_TIMEOUT;
        }
        char probe;
        const size_t room = capacity - *length;
        char *destination = room > 0U ? body + *length : &probe;
        const int amount = (int)(room > BRIDGE_HTTP_READ_CHUNK_BYTES ? BRIDGE_HTTP_READ_CHUNK_BYTES : room);
        const int received = esp_http_client_read(session->client, destination, room > 0U ? amount : 1);
        if (bridge_tls_budget(session, -1) <= 0) {
            return BRIDGE_HTTP_TIMEOUT;
        }
        if (session->wire_limited) {
            return BRIDGE_HTTP_TOO_LARGE;
        }
        if (received < 0) {
            return failure_result(session);
        }
        if (received == 0) {
            /* The SDK reports a transport timeout as a close; a close-delimited body would look complete. */
            if (session->timed_out) {
                return BRIDGE_HTTP_TIMEOUT;
            }
            return esp_http_client_is_complete_data_received(session->client) ? BRIDGE_HTTP_OK : BRIDGE_HTTP_INCOMPLETE;
        }
        if (room == 0U) {
            return BRIDGE_HTTP_TOO_LARGE;
        }
        *length += (size_t)received;
    }
}

/* Headers, status and the complete bounded body of an opened request. */
static bridge_http_result_t receive_response(bridge_tls_t *session, char *body, size_t capacity,
                                             bridge_http_response_t *response)
{
    if (esp_http_client_fetch_headers(session->client) < 0) {
        return failure_result(session);
    }
    if (bridge_tls_budget(session, -1) <= 0) {
        return BRIDGE_HTTP_TIMEOUT;
    }
    response->status = esp_http_client_get_status_code(session->client);
    if (session->encoding_invalid) {
        return BRIDGE_HTTP_INVALID_RESPONSE;
    }
    const int64_t content_length = esp_http_client_get_content_length(session->client);
    if (content_length >= 0 && (uint64_t)content_length > capacity) {
        return BRIDGE_HTTP_TOO_LARGE;
    }
    /* Bounded framing overhead, separate from decoded body and header budgets. */
    session->wire_limit =
        session->wire_bytes + capacity * BRIDGE_HTTP_BODY_WIRE_MULTIPLIER + BRIDGE_HTTP_HEADER_MAX_BYTES;
    return read_body(session, body, capacity, &response->length);
}

static bridge_http_result_t exchange_get(bridge_tls_t *session, const bridge_origin_t *origin, const char *url,
                                         uint32_t io_ms, char *body, size_t capacity,
                                         bridge_http_response_t *response)
{
    if (!bridge_tls_create(session, origin, url, HTTP_METHOD_GET, (int)(io_ms > INT_MAX ? INT_MAX : io_ms))) {
        return BRIDGE_HTTP_NO_MEMORY;
    }
    const bridge_tls_open_t opened = bridge_tls_open(session, 0);
    if (opened == BRIDGE_TLS_GATE_TIMEOUT) {
        return BRIDGE_HTTP_TIMEOUT;
    }
    if (opened == BRIDGE_TLS_OPEN_FAILED) {
        return failure_result(session);
    }
#if CONFIG_BRIDGE_DIAGNOSTIC_PROBE
    memory_report_log("bridge HTTPS open");
#endif
    return receive_response(session, body, capacity, response);
}

bridge_http_response_t bridge_http_get(const bridge_origin_t *origin, const char *path,
                                       uint32_t gate_wait_ms, uint32_t io_ms, char *body, size_t capacity)
{
    bridge_http_response_t response = {.result = BRIDGE_HTTP_INVALID_ARGUMENT};
    char url[BRIDGE_HTTP_URL_SIZE];
    if (body == NULL || capacity == 0U || capacity > BRIDGE_HTTP_BODY_MAX_BYTES) {
        return response;
    }
    memset(body, 0, capacity);
    if (origin == NULL || io_ms == 0U || !bridge_token_valid(origin->token) ||
        !bridge_http_make_url(origin->url, path, url, sizeof(url))) {
        return response;
    }
    if (!bridge_clock_ready()) {
        response.result = BRIDGE_HTTP_CLOCK_NOT_READY;
        return response;
    }
    if (finite_gate == NULL || xSemaphoreTake(finite_gate, pdMS_TO_TICKS(gate_wait_ms)) != pdTRUE) {
        response.result = BRIDGE_HTTP_BUSY;
        return response;
    }
    bridge_tls_t session = {
        .deadline_us = esp_timer_get_time() + (int64_t)io_ms * 1000,
        .wire_limit = BRIDGE_HTTP_HEADER_MAX_BYTES,
    };
    response.result = exchange_get(&session, origin, url, io_ms, body, capacity, &response);
    bridge_tls_destroy(&session);
    xSemaphoreGive(finite_gate);
    if (response.result != BRIDGE_HTTP_OK) {
        memset(body, 0, capacity);
        response.length = 0;
    }
    return response;
}
