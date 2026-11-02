#include "bridge_tls.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "bridge_http.h"
#include "bridge_http_common.h"
#include "esp_crt_bundle.h"
#include "esp_timer.h"
#include "esp_transport_ssl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"

#define BEARER_PREFIX "Bearer "

static void clear_secret(char *secret, size_t size)
{
    volatile char *at = secret;
    while (size-- > 0U) {
        *at++ = 0;
    }
}

int bridge_tls_budget(const bridge_tls_t *session, int requested_ms)
{
    return bridge_http_remaining_ms(session->deadline_us, esp_timer_get_time(), requested_ms);
}

/* Nonblocking TLS keeps a partial record inside the same absolute deadline. */
static int tls_connect(esp_transport_handle_t wrapper, const char *host, int port, int timeout_ms)
{
    bridge_tls_t *session = esp_transport_get_context_data(wrapper);
    for (;;) {
        const int remaining = bridge_tls_budget(session, timeout_ms);
        if (remaining <= (int)portTICK_PERIOD_MS) {
            session->timed_out = true;
            return -1;
        }
        const int result = esp_transport_connect_async(session->tls, host, port, remaining);
        if (result != 0) {
            return result > 0 && bridge_tls_budget(session, timeout_ms) > 0 ? 0 : -1;
        }
        vTaskDelay(1);
    }
}

static int tls_read(esp_transport_handle_t wrapper, char *data, int length, int timeout_ms)
{
    bridge_tls_t *session = esp_transport_get_context_data(wrapper);
    if (!session->streaming) {
        if (session->wire_bytes >= session->wire_limit) {
            session->wire_limited = true;
            return -1;
        }
        const size_t available = session->wire_limit - session->wire_bytes;
        if ((size_t)length > available) {
            length = (int)available;
        }
    }
    if (length > (int)BRIDGE_HTTP_READ_CHUNK_BYTES) {
        length = (int)BRIDGE_HTTP_READ_CHUNK_BYTES;
    }
    for (;;) {
        const int remaining = bridge_tls_budget(session, timeout_ms);
        if (remaining <= (int)portTICK_PERIOD_MS) {
            if (session->streaming) {
                return ERR_TCP_TRANSPORT_CONNECTION_TIMEOUT;
            }
            session->timed_out = true;
            return -1;
        }
        const int result = esp_transport_read(session->tls, data, length, remaining);
        if (result != ERR_TCP_TRANSPORT_CONNECTION_TIMEOUT) {
            if (session->streaming) {
                return result;
            }
            if (result > 0) {
                session->wire_bytes += (size_t)result;
            }
            return bridge_tls_budget(session, timeout_ms) > 0 ? result : -1;
        }
        vTaskDelay(1);
    }
}

static int tls_write(esp_transport_handle_t wrapper, const char *data, int length, int timeout_ms)
{
    bridge_tls_t *session = esp_transport_get_context_data(wrapper);
    const int remaining = bridge_tls_budget(session, timeout_ms);
    if (remaining <= 0) {
        session->timed_out = true;
        return -1;
    }
    const int result = esp_transport_write(session->tls, data, length, remaining);
    return bridge_tls_budget(session, timeout_ms) > 0 ? result : -1;
}

static int tls_close(esp_transport_handle_t wrapper)
{
    bridge_tls_t *session = esp_transport_get_context_data(wrapper);
    return esp_transport_close(session->tls);
}

static esp_err_t on_header(esp_http_client_event_t *event)
{
    bridge_tls_t *session = event->user_data;
    if (event->event_id != HTTP_EVENT_ON_HEADER || event->header_key == NULL || event->header_value == NULL) {
        return ESP_OK;
    }
    if (strcasecmp(event->header_key, "Content-Encoding") == 0 && strcasecmp(event->header_value, "identity") != 0) {
        session->encoding_invalid = true;
    } else if (session->content_type_accepted != NULL && strcasecmp(event->header_key, "Content-Type") == 0) {
        ++session->content_type_count;
        session->content_type_valid = session->content_type_accepted(event->header_value);
    }
    return ESP_OK;
}

static bool set_authorization(esp_http_client_handle_t client, const char *token)
{
    char authorization[sizeof(BEARER_PREFIX) + BRIDGE_TOKEN_MAX_BYTES] = {0};
    snprintf(authorization, sizeof(authorization), BEARER_PREFIX "%s", token);
    const esp_err_t error = esp_http_client_set_header(client, "Authorization", authorization);
    clear_secret(authorization, sizeof(authorization));
    return error == ESP_OK;
}

bool bridge_tls_create(bridge_tls_t *session, const bridge_origin_t *origin, const char *url,
                       esp_http_client_method_t method, int timeout_ms)
{
    session->wrapper = esp_transport_init();
    session->tls = esp_transport_ssl_init();
    if (session->wrapper == NULL || session->tls == NULL) {
        return false;
    }
    esp_transport_ssl_crt_bundle_attach(session->tls, esp_crt_bundle_attach);
    esp_transport_set_context_data(session->wrapper, session);
    if (esp_transport_set_func(session->wrapper, tls_connect, tls_read, tls_write, tls_close, NULL, NULL, NULL) !=
        ESP_OK) {
        return false;
    }
    const esp_http_client_config_t config = {
        .url = url,
        .method = method,
        .transport = session->wrapper,
        .timeout_ms = timeout_ms,
        .disable_auto_redirect = true,
        .max_authorization_retries = -1,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .event_handler = on_header,
        .user_data = session,
        .buffer_size = BRIDGE_HTTP_READ_CHUNK_BYTES,
    };
    session->client = esp_http_client_init(&config);
    return session->client != NULL && set_authorization(session->client, origin->token) &&
           esp_http_client_set_header(session->client, "Accept-Encoding", "identity") == ESP_OK;
}

bridge_tls_open_t bridge_tls_open(bridge_tls_t *session, int write_length)
{
    const int remaining = bridge_tls_budget(session, -1);
    if (remaining <= 0 || !bridge_http_handshake_acquire((uint32_t)remaining)) {
        return BRIDGE_TLS_GATE_TIMEOUT;
    }
    const esp_err_t error = esp_http_client_open(session->client, write_length);
    bridge_http_handshake_release();
    return error == ESP_OK ? BRIDGE_TLS_OPENED : BRIDGE_TLS_OPEN_FAILED;
}

void bridge_tls_disable_delay(bridge_tls_t *session)
{
    const int socket = esp_transport_get_socket(session->tls);
    const int enabled = 1;
    if (socket >= 0) {
        (void)setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, &enabled, sizeof(enabled));
    }
}

void bridge_tls_destroy(bridge_tls_t *session)
{
    if (session->client != NULL) {
        esp_http_client_cleanup(session->client);
        session->client = NULL;
    }
    if (session->wrapper != NULL) {
        esp_transport_destroy(session->wrapper);
        session->wrapper = NULL;
    }
    if (session->tls != NULL) {
        esp_transport_destroy(session->tls);
        session->tls = NULL;
    }
}
