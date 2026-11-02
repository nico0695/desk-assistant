#include "bridge_http.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "bridge_clock.h"
#include "bridge_http_common.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_timer.h"
#include "esp_transport.h"
#include "esp_transport_ssl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#if CONFIG_BRIDGE_DIAGNOSTIC_PROBE
#include "memory_report.h"
#endif

#define BEARER_PREFIX "Bearer "
typedef struct {
    esp_transport_handle_t tls;
    int64_t deadline_us;
    size_t wire_bytes;
    size_t wire_limit;
    bool wire_limited;
    bool timed_out;
    bool encoding_invalid;
} http_transport_t;

static SemaphoreHandle_t finite_gate;
static SemaphoreHandle_t handshake_gate;

static void clear_secret(char *secret, size_t size)
{
    volatile char *at = secret;
    while (size-- > 0U) {
        *at++ = 0;
    }
}

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

static int budget(http_transport_t *context, int requested_ms)
{
    return bridge_http_remaining_ms(context->deadline_us, esp_timer_get_time(), requested_ms);
}

/* Nonblocking TLS keeps a partial record inside the same absolute deadline. */
static int transport_connect(esp_transport_handle_t wrapper, const char *host, int port, int timeout_ms)
{
    http_transport_t *context = esp_transport_get_context_data(wrapper);
    for (;;) {
        const int remaining = budget(context, timeout_ms);
        if (remaining <= (int)portTICK_PERIOD_MS) {
            context->timed_out = true;
            return -1;
        }
        const int result = esp_transport_connect_async(context->tls, host, port, remaining);
        if (result != 0) {
            return result > 0 && budget(context, timeout_ms) > 0 ? 0 : -1;
        }
        vTaskDelay(1);
    }
}

static int transport_read(esp_transport_handle_t wrapper, char *data, int length, int timeout_ms)
{
    http_transport_t *context = esp_transport_get_context_data(wrapper);
    if (context->wire_bytes >= context->wire_limit) {
        context->wire_limited = true;
        return -1;
    }
    if (length > (int)BRIDGE_HTTP_READ_CHUNK_BYTES) {
        length = (int)BRIDGE_HTTP_READ_CHUNK_BYTES;
    }
    const size_t available = context->wire_limit - context->wire_bytes;
    if ((size_t)length > available) {
        length = (int)available;
    }
    for (;;) {
        const int remaining = budget(context, timeout_ms);
        if (remaining <= (int)portTICK_PERIOD_MS) {
            context->timed_out = true;
            return -1;
        }
        const int result = esp_transport_read(context->tls, data, length, remaining);
        if (result != ERR_TCP_TRANSPORT_CONNECTION_TIMEOUT) {
            if (result > 0) {
                context->wire_bytes += (size_t)result;
            }
            return budget(context, timeout_ms) > 0 ? result : -1;
        }
        vTaskDelay(1);
    }
}

static int transport_write(esp_transport_handle_t wrapper, const char *data, int length, int timeout_ms)
{
    http_transport_t *context = esp_transport_get_context_data(wrapper);
    const int remaining = budget(context, timeout_ms);
    if (remaining <= 0) {
        context->timed_out = true;
        return -1;
    }
    const int result = esp_transport_write(context->tls, data, length, remaining);
    return budget(context, timeout_ms) > 0 ? result : -1;
}

static int transport_close(esp_transport_handle_t wrapper)
{
    http_transport_t *context = esp_transport_get_context_data(wrapper);
    return esp_transport_close(context->tls);
}

static esp_err_t on_http_event(esp_http_client_event_t *event)
{
    http_transport_t *context = event->user_data;
    if (event->event_id == HTTP_EVENT_ON_HEADER && event->header_key != NULL && event->header_value != NULL &&
        strcasecmp(event->header_key, "Content-Encoding") == 0 && strcasecmp(event->header_value, "identity") != 0) {
        context->encoding_invalid = true;
    }
    return ESP_OK;
}

static bridge_http_result_t failure_result(const http_transport_t *context)
{
    if (context->wire_limited) {
        return BRIDGE_HTTP_TOO_LARGE;
    }
    return context->timed_out || esp_timer_get_time() >= context->deadline_us ? BRIDGE_HTTP_TIMEOUT : BRIDGE_HTTP_TRANSPORT_ERROR;
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
    if (origin == NULL || io_ms == 0U ||
        !bridge_token_valid(origin->token) || !bridge_http_make_url(origin->url, path, url, sizeof(url))) {
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

    http_transport_t transport = {
        .deadline_us = esp_timer_get_time() + (int64_t)io_ms * 1000,
        .wire_limit = BRIDGE_HTTP_HEADER_MAX_BYTES,
    };
    esp_transport_handle_t wrapper = NULL;
    esp_http_client_handle_t client = NULL;
    char authorization[sizeof(BEARER_PREFIX) + BRIDGE_TOKEN_MAX_BYTES] = {0};
    bool handshake_held = false;
    response.result = BRIDGE_HTTP_NO_MEMORY;
    wrapper = esp_transport_init();
    transport.tls = esp_transport_ssl_init();
    if (wrapper == NULL || transport.tls == NULL) {
        goto cleanup;
    }
    esp_transport_ssl_crt_bundle_attach(transport.tls, esp_crt_bundle_attach);
    esp_transport_set_context_data(wrapper, &transport);
    if (esp_transport_set_func(wrapper, transport_connect, transport_read, transport_write,
                               transport_close, NULL, NULL, NULL) != ESP_OK) {
        goto cleanup;
    }
    const esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .transport = wrapper,
        .timeout_ms = (int)(io_ms > INT_MAX ? INT_MAX : io_ms),
        .disable_auto_redirect = true,
        .max_authorization_retries = -1,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .event_handler = on_http_event,
        .user_data = &transport,
        .buffer_size = BRIDGE_HTTP_READ_CHUNK_BYTES,
    };
    client = esp_http_client_init(&config);
    if (client == NULL) {
        goto cleanup;
    }
    snprintf(authorization, sizeof(authorization), BEARER_PREFIX "%s", origin->token);
    const esp_err_t header_error = esp_http_client_set_header(client, "Authorization", authorization);
    clear_secret(authorization, sizeof(authorization));
    if (header_error != ESP_OK || esp_http_client_set_header(client, "Accept-Encoding", "identity") != ESP_OK) {
        goto cleanup;
    }
    const int remaining = budget(&transport, -1);
    if (remaining <= 0 || !bridge_http_handshake_acquire((uint32_t)remaining)) {
        response.result = BRIDGE_HTTP_TIMEOUT;
        goto cleanup;
    }
    handshake_held = true;
    const esp_err_t open_error = esp_http_client_open(client, 0);
    bridge_http_handshake_release();
    handshake_held = false;
    if (open_error != ESP_OK) {
        response.result = failure_result(&transport);
        goto cleanup;
    }
#if CONFIG_BRIDGE_DIAGNOSTIC_PROBE
    memory_report_log("bridge HTTPS open");
#endif
    if (esp_http_client_fetch_headers(client) < 0) {
        response.result = failure_result(&transport);
        goto cleanup;
    }
    if (budget(&transport, -1) <= 0) {
        response.result = BRIDGE_HTTP_TIMEOUT;
        goto cleanup;
    }
    response.status = esp_http_client_get_status_code(client);
    if (transport.encoding_invalid) {
        response.result = BRIDGE_HTTP_INVALID_RESPONSE;
        goto cleanup;
    }
    const int64_t content_length = esp_http_client_get_content_length(client);
    if (content_length >= 0 && (uint64_t)content_length > capacity) {
        response.result = BRIDGE_HTTP_TOO_LARGE;
        goto cleanup;
    }
    /* Bounded framing overhead, separate from decoded body and header budgets. */
    transport.wire_limit = transport.wire_bytes + capacity * BRIDGE_HTTP_BODY_WIRE_MULTIPLIER + BRIDGE_HTTP_HEADER_MAX_BYTES;
    for (;;) {
        if (budget(&transport, -1) <= 0) {
            response.result = BRIDGE_HTTP_TIMEOUT;
            goto cleanup;
        }
        char probe;
        const size_t room = capacity - response.length;
        char *destination = room > 0U ? body + response.length : &probe;
        const int amount = (int)(room > BRIDGE_HTTP_READ_CHUNK_BYTES ? BRIDGE_HTTP_READ_CHUNK_BYTES : room);
        const int received = esp_http_client_read(client, destination, room > 0U ? amount : 1);
        if (budget(&transport, -1) <= 0) {
            response.result = BRIDGE_HTTP_TIMEOUT;
            goto cleanup;
        }
        if (transport.wire_limited) {
            response.result = BRIDGE_HTTP_TOO_LARGE;
            goto cleanup;
        }
        if (received < 0) {
            response.result = failure_result(&transport);
            goto cleanup;
        }
        if (received == 0) {
            /* The SDK reports a transport timeout as a close; a close-delimited body would look complete. */
            if (transport.timed_out) {
                response.result = BRIDGE_HTTP_TIMEOUT;
            } else {
                response.result = esp_http_client_is_complete_data_received(client) ? BRIDGE_HTTP_OK : BRIDGE_HTTP_INCOMPLETE;
            }
            break;
        }
        if (room == 0U) {
            response.result = BRIDGE_HTTP_TOO_LARGE;
            goto cleanup;
        }
        response.length += (size_t)received;
    }

cleanup:
    if (handshake_held) {
        bridge_http_handshake_release();
    }
    if (client != NULL) {
        esp_http_client_cleanup(client);
    }
    if (wrapper != NULL) {
        esp_transport_destroy(wrapper);
    }
    if (transport.tls != NULL) {
        esp_transport_destroy(transport.tls);
    }
    clear_secret(authorization, sizeof(authorization));
    xSemaphoreGive(finite_gate);
    if (response.result != BRIDGE_HTTP_OK) {
        memset(body, 0, capacity);
        response.length = 0;
    }
    return response;
}
