#include "bridge_stream.h"

#include <strings.h>

#include "bridge_http.h"
#include "bridge_http_common.h"
#include "bridge_tls.h"
#include "esp_http_client.h"
#include "esp_timer.h"

#define STREAM_ROUTE "/v1/device/event-stream"
#define STREAM_READ_MS 1000U
#define STREAM_READ_BYTES 256U

static bool stream_content_type(const char *value)
{
    static const char type[] = "text/event-stream";
    if (strncasecmp(value, type, sizeof(type) - 1U) != 0) return false;
    value += sizeof(type) - 1U;
    while (*value == ' ' || *value == '\t') ++value;
    return *value == '\0' || *value == ';';
}

/* Connects and validates the response headers; BRIDGE_STREAM_STOPPED means the stream may be read. */
static bridge_stream_result_t open_stream(bridge_tls_t *session, const bridge_origin_t *origin)
{
    bridge_stream_result_t result = {.reason = BRIDGE_STREAM_SETUP_FAILED};
    char url[BRIDGE_HTTP_URL_SIZE];
    if (!bridge_http_make_url(origin->url, STREAM_ROUTE, url, sizeof(url)) ||
        !bridge_tls_create(session, origin, url, HTTP_METHOD_GET, BRIDGE_SSE_OPEN_MS) ||
        esp_http_client_set_header(session->client, "Accept", "text/event-stream") != ESP_OK) {
        return result;
    }
    result.reason = BRIDGE_STREAM_CONNECT_FAILED;
    if (bridge_tls_open(session, 0) != BRIDGE_TLS_OPENED) return result;
    result.reason = BRIDGE_STREAM_HEADERS_FAILED;
    if (esp_http_client_fetch_headers(session->client) < 0 || bridge_tls_budget(session, -1) <= 0) return result;
    result.http_status = esp_http_client_get_status_code(session->client);
    result.reason = BRIDGE_STREAM_HTTP_REJECTED;
    if (result.http_status != 200) return result;
    result.reason = BRIDGE_STREAM_INVALID_HEADERS;
    if (session->content_type_count != 1U || !session->content_type_valid || session->encoding_invalid) return result;
    return (bridge_stream_result_t){.reason = BRIDGE_STREAM_STOPPED};
}

static uint64_t now_ms(void)
{
    return (uint64_t)esp_timer_get_time() / 1000U;
}

/* Feeds the parser until the stream dies, fails or stops being current. */
static bridge_stream_reason_t read_stream(bridge_tls_t *session, bridge_sse_t *state,
                                          const bridge_stream_hooks_t *hooks)
{
    char bytes[STREAM_READ_BYTES];
    while (hooks->current(hooks->context)) {
        if (bridge_sse_dead(state, now_ms())) return BRIDGE_STREAM_HEARTBEAT_TIMEOUT;
        session->deadline_us = esp_timer_get_time() + (int64_t)STREAM_READ_MS * 1000;
        const int64_t dead_us = (int64_t)(state->last_bytes_ms + BRIDGE_SSE_DEAD_MS) * 1000;
        if (dead_us < session->deadline_us) session->deadline_us = dead_us;
        const int count = esp_http_client_read(session->client, bytes, sizeof(bytes));
        if (count > 0) {
            if (!bridge_sse_feed(state, bytes, (size_t)count, now_ms())) return BRIDGE_STREAM_INVALID_FRAME;
        } else if (count != -ESP_ERR_HTTP_EAGAIN) {
            return BRIDGE_STREAM_READ_FAILED;
        }
    }
    return BRIDGE_STREAM_STOPPED;
}

bridge_stream_result_t bridge_stream_run(const bridge_origin_t *origin, bridge_sse_t *state,
                                         const bridge_stream_hooks_t *hooks)
{
    bridge_tls_t session = {
        .deadline_us = esp_timer_get_time() + (int64_t)BRIDGE_SSE_OPEN_MS * 1000,
        .wire_limit = BRIDGE_HTTP_HEADER_MAX_BYTES,
        .content_type_accepted = stream_content_type,
    };
    bridge_stream_result_t result = open_stream(&session, origin);
    if (result.reason == BRIDGE_STREAM_STOPPED && hooks->opened(hooks->context)) {
        session.streaming = true;
        bridge_sse_connected(state, now_ms());
        result.reason = read_stream(&session, state, hooks);
    }
    bridge_tls_destroy(&session);
    return result;
}
