#include "bridge_stream.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "bridge_http.h"
#include "bridge_http_common.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_timer.h"
#include "esp_transport.h"
#include "esp_transport_ssl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define STREAM_ROUTE "/v1/device/event-stream"
#define STREAM_READ_MS 1000U
#define STREAM_READ_BYTES 256U
#define BEARER_PREFIX "Bearer "

typedef struct {
    esp_transport_handle_t tls;
    int64_t deadline_us;
    size_t header_bytes;
    unsigned content_type_count;
    bool content_type_valid;
    bool encoding_invalid;
    bool streaming;
} stream_transport_t;

static int remaining(stream_transport_t *stream, int requested)
{
    return bridge_http_remaining_ms(stream->deadline_us, esp_timer_get_time(), requested);
}

static int stream_connect(esp_transport_handle_t wrapper, const char *host, int port, int timeout)
{
    stream_transport_t *stream = esp_transport_get_context_data(wrapper);
    for (;;) {
        int budget = remaining(stream, timeout);
        if (budget <= (int)portTICK_PERIOD_MS) return -1;
        int result = esp_transport_connect_async(stream->tls, host, port, budget);
        if (result != 0) return result > 0 && remaining(stream, timeout) > 0 ? 0 : -1;
        vTaskDelay(1);
    }
}

static int stream_read(esp_transport_handle_t wrapper, char *buffer, int length, int timeout)
{
    stream_transport_t *stream = esp_transport_get_context_data(wrapper);
    if (!stream->streaming) {
        if (stream->header_bytes >= BRIDGE_HTTP_HEADER_MAX_BYTES) return -1;
        size_t room = BRIDGE_HTTP_HEADER_MAX_BYTES - stream->header_bytes;
        if ((size_t)length > room) length = (int)room;
    }
    if (length > (int)BRIDGE_HTTP_READ_CHUNK_BYTES) length = BRIDGE_HTTP_READ_CHUNK_BYTES;
    for (;;) {
        int budget = remaining(stream, timeout);
        if (budget <= (int)portTICK_PERIOD_MS) {
            return stream->streaming ? ERR_TCP_TRANSPORT_CONNECTION_TIMEOUT : -1;
        }
        int result = esp_transport_read(stream->tls, buffer, length, budget);
        if (result != ERR_TCP_TRANSPORT_CONNECTION_TIMEOUT) {
            if (result > 0 && !stream->streaming) stream->header_bytes += (size_t)result;
            if (!stream->streaming && remaining(stream, timeout) <= 0) return -1;
            return result;
        }
        vTaskDelay(1);
    }
}

static int stream_write(esp_transport_handle_t wrapper, const char *buffer, int length, int timeout)
{
    stream_transport_t *stream = esp_transport_get_context_data(wrapper);
    int budget = remaining(stream, timeout);
    if (budget <= 0) return -1;
    int result = esp_transport_write(stream->tls, buffer, length, budget);
    return remaining(stream, timeout) > 0 ? result : -1;
}

static int stream_close(esp_transport_handle_t wrapper)
{
    stream_transport_t *stream = esp_transport_get_context_data(wrapper);
    return esp_transport_close(stream->tls);
}

static bool stream_content_type(const char *value)
{
    static const char type[] = "text/event-stream";
    if (strncasecmp(value, type, sizeof(type) - 1U) != 0) return false;
    value += sizeof(type) - 1U;
    while (*value == ' ' || *value == '\t') ++value;
    return *value == '\0' || *value == ';';
}

static esp_err_t stream_event(esp_http_client_event_t *event)
{
    stream_transport_t *stream = event->user_data;
    if (event->event_id != HTTP_EVENT_ON_HEADER || event->header_key == NULL || event->header_value == NULL) {
        return ESP_OK;
    }
    if (strcasecmp(event->header_key, "Content-Type") == 0) {
        ++stream->content_type_count;
        stream->content_type_valid = stream_content_type(event->header_value);
    } else if (strcasecmp(event->header_key, "Content-Encoding") == 0 &&
               strcasecmp(event->header_value, "identity") != 0) {
        stream->encoding_invalid = true;
    }
    return ESP_OK;
}

static void clear_secret(char *secret, size_t size)
{
    volatile char *at = secret;
    while (size-- > 0U) *at++ = 0;
}

void bridge_stream_run(const bridge_origin_t *origin, bridge_sse_t *state, const bridge_stream_hooks_t *hooks)
{
    stream_transport_t stream = {.deadline_us = esp_timer_get_time() + (int64_t)BRIDGE_SSE_OPEN_MS * 1000};
    esp_transport_handle_t wrapper = NULL;
    esp_http_client_handle_t client = NULL;
    char authorization[sizeof(BEARER_PREFIX) + BRIDGE_TOKEN_MAX_BYTES] = {0};
    char url[BRIDGE_HTTP_URL_SIZE];
    bool handshake_held = false;
    if (!bridge_http_make_url(origin->url, STREAM_ROUTE, url, sizeof(url))) goto cleanup;
    wrapper = esp_transport_init();
    stream.tls = esp_transport_ssl_init();
    if (wrapper == NULL || stream.tls == NULL) goto cleanup;
    esp_transport_ssl_crt_bundle_attach(stream.tls, esp_crt_bundle_attach);
    esp_transport_set_context_data(wrapper, &stream);
    if (esp_transport_set_func(wrapper, stream_connect, stream_read, stream_write, stream_close,
                               NULL, NULL, NULL) != ESP_OK) goto cleanup;
    const esp_http_client_config_t config = {
        .url = url, .method = HTTP_METHOD_GET, .transport = wrapper, .timeout_ms = BRIDGE_SSE_OPEN_MS,
        .disable_auto_redirect = true, .max_authorization_retries = -1,
        .crt_bundle_attach = esp_crt_bundle_attach, .event_handler = stream_event,
        .user_data = &stream, .buffer_size = BRIDGE_HTTP_READ_CHUNK_BYTES,
    };
    client = esp_http_client_init(&config);
    if (client == NULL) goto cleanup;
    snprintf(authorization, sizeof(authorization), BEARER_PREFIX "%s", origin->token);
    esp_err_t error = esp_http_client_set_header(client, "Authorization", authorization);
    clear_secret(authorization, sizeof(authorization));
    if (error != ESP_OK || esp_http_client_set_header(client, "Accept", "text/event-stream") != ESP_OK ||
        esp_http_client_set_header(client, "Accept-Encoding", "identity") != ESP_OK) goto cleanup;
    int budget = remaining(&stream, -1);
    if (budget <= 0 || !bridge_http_handshake_acquire((uint32_t)budget)) goto cleanup;
    handshake_held = true;
    error = esp_http_client_open(client, 0);
    bridge_http_handshake_release();
    handshake_held = false;
    if (error != ESP_OK || esp_http_client_fetch_headers(client) < 0 ||
        remaining(&stream, -1) <= 0 || esp_http_client_get_status_code(client) != 200 ||
        stream.content_type_count != 1U || !stream.content_type_valid || stream.encoding_invalid ||
        !hooks->opened(hooks->context)) goto cleanup;
    stream.streaming = true;
    bridge_sse_connected(state, (uint64_t)esp_timer_get_time() / 1000U);
    char bytes[STREAM_READ_BYTES];
    while (hooks->current(hooks->context)) {
        uint64_t now = (uint64_t)esp_timer_get_time() / 1000U;
        if (bridge_sse_dead(state, now)) break;
        stream.deadline_us = esp_timer_get_time() + (int64_t)STREAM_READ_MS * 1000;
        int64_t dead_us = (int64_t)(state->last_bytes_ms + BRIDGE_SSE_DEAD_MS) * 1000;
        if (dead_us < stream.deadline_us) stream.deadline_us = dead_us;
        int count = esp_http_client_read(client, bytes, sizeof(bytes));
        if (count > 0) {
            if (!bridge_sse_feed(state, bytes, (size_t)count, (uint64_t)esp_timer_get_time() / 1000U)) break;
        } else if (count != -ESP_ERR_HTTP_EAGAIN) {
            break;
        }
    }
cleanup:
    if (handshake_held) bridge_http_handshake_release();
    if (client != NULL) esp_http_client_cleanup(client);
    if (wrapper != NULL) esp_transport_destroy(wrapper);
    if (stream.tls != NULL) esp_transport_destroy(stream.tls);
    clear_secret(authorization, sizeof(authorization));
}
