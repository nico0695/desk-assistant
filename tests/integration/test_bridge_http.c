#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bridge_clock.h"
#include "bridge_http.h"
#include "bridge_stream.h"
#include "fake_sdk.h"
#include "lwip/sockets.h"
#include "unity.h"
#include "../../components/bridge/src/bridge_http.c"

typedef enum { NORMAL, DENIED, TRUNCATED, RESPONSE_OVERFLOW, SLOW, LONG_HEADERS, FALSE_EOF, BAD_ENCODING, CONNECT_STALL,
               STREAM_OK, STREAM_NO_TYPE, STREAM_DENIED, STREAM_IDLE, STREAM_ENCODED, STREAM_TWO_TYPES,
               DEADLINE_CLOSE, PUT_OK, PUT_FAIL_FIRST_WRITE, PUT_FAIL_LAST_WRITE, PUT_STALL, PUT_SLOW,
               JSON_ACK, JSON_SNOOZE, ACTION } fake_mode_t;
struct fake_transport { void *context; connect_fn connect; read_fn read; write_fn write; close_fn close; bool tls; };
struct fake_client { esp_http_client_config_t config; size_t offset; bool completed; };
static fake_mode_t mode;
static bool clock_ready;
static int64_t now_us;
static unsigned dispatches, created, destroyed, client_count, client_freed;
static bool bundle_set, auth_set, finite_busy, handshake_busy;
static unsigned fail_transport_call;
static bool fail_client, fail_header, fail_open, fail_write, content_type_set;
static char captured_url[BRIDGE_URL_SIZE + 160U];
static struct fake_client *current;
static esp_http_client_method_t current_method;
static const char *payload = "{\"events\":[]}";
static bridge_origin_t origin = {"https://example.test/prefix/", "01234567890123456789012345678901"};
#define UPLOAD_BYTES 20000U
static size_t uploaded, last_write_offset, source_reads, fail_source_at;
static bool nodelay_set;
static unsigned gate_releases;
static void gate_released(void)
{
    TEST_ASSERT_FALSE(*(bool *)finite_gate);
    ++gate_releases;
}

int fake_setsockopt(int socket, int level, int option, const void *value, unsigned length)
{
    (void)socket; (void)value; (void)length;
    nodelay_set = level == IPPROTO_TCP && option == TCP_NODELAY;
    return 0;
}
int esp_transport_get_socket(esp_transport_handle_t t) { (void)t; return 3; }

bool bridge_clock_ready(void) { return clock_ready; }
int64_t esp_timer_get_time(void) { return now_us; }
void vTaskDelay(TickType_t ticks) { now_us += (int64_t)ticks * 1000; }
esp_err_t esp_crt_bundle_attach(void *context) { (void)context; return ESP_OK; }
SemaphoreHandle_t xSemaphoreCreateMutex(void) { bool *gate = calloc(1, sizeof(*gate)); return gate; }
int xSemaphoreTake(SemaphoreHandle_t gate, TickType_t wait)
{
    (void)wait;
    if (*(bool *)gate || (gate == finite_gate && finite_busy) || (gate == handshake_gate && handshake_busy)) return 0;
    *(bool *)gate = true;
    return pdTRUE;
}
int xSemaphoreGive(SemaphoreHandle_t gate) { *(bool *)gate = false; return pdTRUE; }
void vSemaphoreDelete(SemaphoreHandle_t gate) { free(gate); }
esp_transport_handle_t esp_transport_init(void)
{
    if (fail_transport_call != 0U && --fail_transport_call == 0U) return NULL;
    ++created; return calloc(1, sizeof(struct fake_transport));
}
esp_transport_handle_t esp_transport_ssl_init(void) { esp_transport_handle_t t = esp_transport_init(); if (t != NULL) t->tls = true; return t; }
esp_err_t esp_transport_destroy(esp_transport_handle_t t) { ++destroyed; free(t); return ESP_OK; }
void *esp_transport_get_context_data(esp_transport_handle_t t) { return t->context; }
esp_err_t esp_transport_set_context_data(esp_transport_handle_t t, void *data) { t->context = data; return ESP_OK; }
esp_err_t esp_transport_set_func(esp_transport_handle_t t, connect_fn c, read_fn r, write_fn w,
                                close_fn close, void *a, void *b, void *d)
{
    (void)a; (void)b; (void)d; t->connect = c; t->read = r; t->write = w; t->close = close; return ESP_OK;
}
void esp_transport_ssl_crt_bundle_attach(esp_transport_handle_t t, esp_err_t (*attach)(void *))
{
    TEST_ASSERT_TRUE(t->tls); TEST_ASSERT_TRUE(attach == esp_crt_bundle_attach); bundle_set = true;
}
int esp_transport_connect_async(esp_transport_handle_t t, const char *host, int port, int timeout)
{
    (void)host; (void)port; TEST_ASSERT_TRUE(t->tls); TEST_ASSERT_GREATER_THAN(0, timeout);
    return mode == CONNECT_STALL ? 0 : 1;
}
int esp_transport_write(esp_transport_handle_t t, const char *data, int length, int timeout)
{
    (void)t; (void)data; (void)timeout; return length;
}
int esp_transport_close(esp_transport_handle_t t) { (void)t; return 0; }
int esp_transport_read(esp_transport_handle_t t, char *data, int length, int timeout)
{
    (void)t; (void)timeout;
    if (mode == STREAM_IDLE) return ERR_TCP_TRANSPORT_CONNECTION_TIMEOUT;
    if (mode == LONG_HEADERS || mode == FALSE_EOF) { memset(data, 'h', (size_t)length); return length; }
    if (mode == SLOW) now_us += INT64_C(600000);
    size_t available = strlen(payload) - current->offset;
    if ((size_t)length > available) length = (int)available;
    memcpy(data, payload + current->offset, (size_t)length);
    current->offset += (size_t)length;
    return length;
}
esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t *config)
{
    if (fail_client) return NULL;
    ++client_count;
    TEST_ASSERT_TRUE(config->disable_auto_redirect);
    TEST_ASSERT_EQUAL_INT(-1, config->max_authorization_retries);
    TEST_ASSERT_TRUE(bundle_set);
    snprintf(captured_url, sizeof(captured_url), "%s", config->url);
    current = calloc(1, sizeof(*current)); current->config = *config; current_method = config->method; return current;
}
esp_err_t esp_http_client_set_header(esp_http_client_handle_t c, const char *key, const char *value)
{
    (void)c;
    if (strcmp(key, "Authorization") == 0) {
        if (fail_header) return ESP_FAIL;
        TEST_ASSERT_EQUAL_STRING("Bearer 01234567890123456789012345678901", value); auth_set = true;
    }
    if (strcmp(key, "Content-Type") == 0) content_type_set = true;
    return ESP_OK;
}
esp_err_t esp_http_client_open(esp_http_client_handle_t c, int length)
{
    TEST_ASSERT_EQUAL_INT(mode == JSON_SNOOZE ? (int)strlen("{\"preset\":\"snooze_5m\"}") :
                          mode == JSON_ACK ? (int)strlen(BRIDGE_EVENT_ACK_BODY) :
                          c->config.method == HTTP_METHOD_PUT ? (int)UPLOAD_BYTES : 0, length);
    ++dispatches;
    if (fail_open) return ESP_FAIL;
    if (mode == PUT_SLOW) now_us += INT64_C(29000000);
    return c->config.transport->connect(c->config.transport, "example.test", 443, c->config.timeout_ms) == 0 ? ESP_OK : ESP_FAIL;
}
int64_t esp_http_client_fetch_headers(esp_http_client_handle_t c)
{
    if (mode == STREAM_OK || mode == STREAM_DENIED || mode == STREAM_IDLE || mode == STREAM_ENCODED ||
        mode == STREAM_TWO_TYPES) {
        esp_http_client_event_t event = {.user_data = c->config.user_data, .event_id = HTTP_EVENT_ON_HEADER,
                                         .header_key = "Content-Type", .header_value = "text/event-stream"};
        c->config.event_handler(&event);
        if (mode == STREAM_TWO_TYPES) c->config.event_handler(&event);
        if (mode == STREAM_ENCODED) {
            event.header_key = "Content-Encoding";
            event.header_value = "gzip";
            c->config.event_handler(&event);
        }
    }
    if (mode == LONG_HEADERS) {
        char bytes[1024];
        while (c->config.transport->read(c->config.transport, bytes, sizeof(bytes), 5000) > 0) {}
        return -1;
    }
    if (mode == BAD_ENCODING) {
        esp_http_client_event_t event = {.user_data = c->config.user_data, .event_id = HTTP_EVENT_ON_HEADER,
                                         .header_key = "Content-Encoding", .header_value = "gzip"};
        c->config.event_handler(&event);
    }
    return mode == JSON_ACK ? 0 : (int64_t)strlen(payload);
}
int esp_http_client_get_status_code(esp_http_client_handle_t c)
{
    (void)c;
    if (mode == PUT_OK) return 202;
    if (mode == JSON_ACK) return 204;
    return mode == DENIED || mode == STREAM_DENIED ? 401 : 200;
}
int64_t esp_http_client_get_content_length(esp_http_client_handle_t c)
{
    (void)c; return mode == JSON_ACK ? 0 :
                     mode == RESPONSE_OVERFLOW || mode == DEADLINE_CLOSE ? -1 : (int64_t)strlen(payload);
}
int esp_http_client_read(esp_http_client_handle_t c, char *data, int length)
{
    if (mode == JSON_ACK) { c->completed = true; return 0; }
    if (mode == STREAM_IDLE) {
        int64_t before = now_us;
        int result = c->config.transport->read(c->config.transport, data, length, c->config.timeout_ms);
        if (now_us == before) now_us += 1000;
        return result == ERR_TCP_TRANSPORT_CONNECTION_TIMEOUT ? -ESP_ERR_HTTP_EAGAIN : result;
    }
    if (mode == STREAM_OK) {
        static const char frame[] = "event: pending_events\ndata: {}\n\n";
        if (c->offset == 0U) {
            TEST_ASSERT_GREATER_OR_EQUAL(sizeof(frame) - 1U, (size_t)length);
            memcpy(data, frame, sizeof(frame) - 1U);
            c->offset = 1U;
            return (int)(sizeof(frame) - 1U);
        }
        now_us += 1000000;
        return -ESP_ERR_HTTP_EAGAIN;
    }
    if (mode == DEADLINE_CLOSE) {
        /* Close-delimited body whose last read reaches the deadline; the SDK reports it as a close. */
        bridge_tls_t *transport = esp_transport_get_context_data(c->config.transport);
        now_us = transport->deadline_us - 1000;
        TEST_ASSERT_EQUAL_INT(-1, c->config.transport->read(c->config.transport, data, length, c->config.timeout_ms));
        c->completed = true;
        return 0;
    }
    if (mode == FALSE_EOF) {
        char framing[1024];
        while (c->config.transport->read(c->config.transport, framing, sizeof(framing), 5000) > 0) {}
        c->completed = true;
        return 0;
    }
    if (c->offset == strlen(payload)) { c->completed = mode != TRUNCATED; return 0; }
    if (mode == SLOW && length > 1) length = 1;
    return c->config.transport->read(c->config.transport, data, length, c->config.timeout_ms);
}
bool esp_http_client_is_complete_data_received(esp_http_client_handle_t c) { return c->completed; }
int esp_http_client_write(esp_http_client_handle_t c, const char *data, int length)
{
    TEST_ASSERT_NOT_EQUAL_INT(ACTION, mode);
    if (fail_write) return -1;
    if (mode == JSON_ACK) {
        TEST_ASSERT_EQUAL_INT(HTTP_METHOD_PUT, c->config.method);
        TEST_ASSERT_EQUAL_INT((int)strlen(BRIDGE_EVENT_ACK_BODY), length);
        TEST_ASSERT_EQUAL_MEMORY(BRIDGE_EVENT_ACK_BODY, data, (size_t)length);
        return length;
    }
    if (mode == JSON_SNOOZE) {
        TEST_ASSERT_EQUAL_INT(HTTP_METHOD_POST, c->config.method);
        TEST_ASSERT_EQUAL_MEMORY("{\"preset\":\"snooze_5m\"}", data, (size_t)length);
        return length;
    }
    TEST_ASSERT_EQUAL_INT(HTTP_METHOD_PUT, c->config.method);
    TEST_ASSERT_LESS_OR_EQUAL_INT((int)BRIDGE_ASSISTANT_UPLOAD_BLOCK_BYTES, length);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)uploaded, (uint8_t)data[0]);
    const bool last = uploaded + (size_t)length == UPLOAD_BYTES;
    if ((mode == PUT_FAIL_FIRST_WRITE && uploaded == 0U) || (mode == PUT_FAIL_LAST_WRITE && last)) return -1;
    if (mode == PUT_STALL) now_us += INT64_C(31000000);
    if (mode == PUT_SLOW) now_us += INT64_C(25000000);
    if (last) last_write_offset = uploaded;
    /* Accepts half of a block at a time to exercise partial writes. */
    if (length > 1) length /= 2;
    return c->config.transport->write(c->config.transport, data, length, c->config.timeout_ms) == length
               ? (uploaded += (size_t)length, length) : -1;
}
esp_err_t esp_http_client_cleanup(esp_http_client_handle_t c) { ++client_freed; free(c); current = NULL; return ESP_OK; }

void setUp(void)
{
    uploaded = last_write_offset = source_reads = fail_source_at = 0; nodelay_set = false;
    mode = NORMAL; clock_ready = true; now_us = 0; dispatches = created = destroyed = client_count = client_freed = 0;
    payload = "{\"events\":[]}";
    bundle_set = auth_set = finite_busy = handshake_busy = false;
    fail_transport_call = 0; fail_client = fail_header = fail_open = fail_write = content_type_set = false;
    gate_releases = 0;
    bridge_http_set_gate_released(NULL);
}
void tearDown(void)
{
    TEST_ASSERT_EQUAL_UINT(created, destroyed);
    TEST_ASSERT_EQUAL_UINT(client_count, client_freed);
}

static void complete_get_with_prefix_and_auth(void)
{
    char body[64];
    bridge_http_response_t result = bridge_http_get(&origin, "/v1/device/events?limit=1", 5000, 5000, body, sizeof(body));
    TEST_ASSERT_EQUAL_INT(BRIDGE_HTTP_OK, result.result);
    TEST_ASSERT_EQUAL_INT(200, result.status);
    TEST_ASSERT_EQUAL_UINT(strlen(payload), result.length);
    TEST_ASSERT_EQUAL_MEMORY(payload, body, result.length);
    TEST_ASSERT_EQUAL_STRING("https://example.test/prefix/v1/device/events?limit=1", captured_url);
    TEST_ASSERT_TRUE(auth_set); TEST_ASSERT_EQUAL_UINT(1, dispatches);
}

static void ack_json_is_one_put_with_empty_204(void)
{
    mode = JSON_ACK;
    char response[1];
    bridge_http_response_t result = bridge_http_json(&origin,
        "/v1/device/events/01234567-89ab-cdef-0123-456789abcdef/ack", BRIDGE_HTTP_PUT_JSON,
        BRIDGE_EVENT_ACK_BODY, strlen(BRIDGE_EVENT_ACK_BODY), BRIDGE_HTTP_EVENT_GATE_MS,
        BRIDGE_HTTP_EVENT_IO_MS, response, sizeof(response)).response;
    TEST_ASSERT_EQUAL_INT(BRIDGE_HTTP_OK, result.result);
    TEST_ASSERT_EQUAL_INT(204, result.status);
    TEST_ASSERT_EQUAL_UINT(0, result.length);
    TEST_ASSERT_EQUAL_UINT(1, dispatches);
    TEST_ASSERT_EQUAL_STRING("https://example.test/prefix/v1/device/events/01234567-89ab-cdef-0123-456789abcdef/ack",
                             captured_url);
}

static void snooze_json_is_one_post_with_bounded_response(void)
{
    mode = JSON_SNOOZE;
    payload = "{\"alert_id\":42,\"scheduled_at\":\"2026-11-02T13:00:00Z\"}";
    char response[128];
    const char *request = "{\"preset\":\"snooze_5m\"}";
    const bridge_http_mutation_t result = bridge_http_json(&origin, "/v1/device/alerts/42/snooze",
        BRIDGE_HTTP_POST_JSON, request, strlen(request), 5000U, 40000U, response, sizeof(response));
    TEST_ASSERT_EQUAL_INT(BRIDGE_HTTP_OK, result.response.result);
    TEST_ASSERT_EQUAL_INT(200, result.response.status);
    TEST_ASSERT_EQUAL_UINT(strlen(payload), result.response.length);
    TEST_ASSERT_EQUAL_MEMORY(payload, response, result.response.length);
    TEST_ASSERT_FALSE(result.unsent);
    TEST_ASSERT_TRUE(content_type_set);
    TEST_ASSERT_EQUAL_UINT(1, dispatches);
}

static bridge_http_mutation_t action(bridge_http_action_method_t method)
{
    static char response[64];
    return bridge_http_action(&origin, "/v1/device/tasks/7/complete", method, 5000U, 60000U, response,
                              sizeof(response));
}

static void actions_send_no_body_or_content_type(void)
{
    mode = ACTION;
    payload = "{\"id\":7,\"status\":\"completed\"}";
    bridge_http_mutation_t result = action(BRIDGE_HTTP_POST_EMPTY);
    TEST_ASSERT_EQUAL_INT(BRIDGE_HTTP_OK, result.response.result);
    TEST_ASSERT_EQUAL_INT(200, result.response.status);
    TEST_ASSERT_EQUAL_UINT(strlen(payload), result.response.length);
    TEST_ASSERT_FALSE(result.unsent);
    TEST_ASSERT_FALSE(content_type_set);
    TEST_ASSERT_EQUAL_STRING("https://example.test/prefix/v1/device/tasks/7/complete", captured_url);
    TEST_ASSERT_EQUAL_INT(HTTP_METHOD_POST, current_method);
    result = action(BRIDGE_HTTP_DELETE);
    TEST_ASSERT_EQUAL_INT(BRIDGE_HTTP_OK, result.response.result);
    TEST_ASSERT_EQUAL_INT(HTTP_METHOD_DELETE, current_method);
    TEST_ASSERT_FALSE(content_type_set);
    TEST_ASSERT_EQUAL_UINT(2, dispatches);
    TEST_ASSERT_EQUAL_INT(BRIDGE_HTTP_INVALID_ARGUMENT, action((bridge_http_action_method_t)9).response.result);
    TEST_ASSERT_EQUAL_UINT(2, dispatches);
}

/* A request is unsent until it is written; after that any failure is uncertain. */
static void mutations_are_unsent_only_before_the_request_is_written(void)
{
    mode = ACTION;
    finite_busy = true;
    bridge_http_mutation_t result = action(BRIDGE_HTTP_DELETE);
    TEST_ASSERT_EQUAL_INT(BRIDGE_HTTP_BUSY, result.response.result);
    TEST_ASSERT_TRUE(result.unsent);
    finite_busy = false; fail_open = true;
    result = action(BRIDGE_HTTP_DELETE);
    TEST_ASSERT_EQUAL_INT(BRIDGE_HTTP_TRANSPORT_ERROR, result.response.result);
    TEST_ASSERT_TRUE(result.unsent);
    fail_open = false; mode = TRUNCATED;
    result = action(BRIDGE_HTTP_DELETE);
    TEST_ASSERT_EQUAL_INT(BRIDGE_HTTP_INCOMPLETE, result.response.result);
    TEST_ASSERT_FALSE(result.unsent);
    mode = JSON_SNOOZE; fail_write = true;
    char response[64];
    const char *request = "{\"preset\":\"snooze_5m\"}";
    result = bridge_http_json(&origin, "/v1/device/alerts/42/snooze", BRIDGE_HTTP_POST_JSON, request,
                              strlen(request), 5000U, 40000U, response, sizeof(response));
    TEST_ASSERT_EQUAL_INT(BRIDGE_HTTP_TRANSPORT_ERROR, result.response.result);
    TEST_ASSERT_FALSE(result.unsent);
    TEST_ASSERT_EQUAL_UINT(3, dispatches);
}

static void finite_gate_release_wakes_a_waiting_owner(void)
{
    char body[64];
    bridge_http_set_gate_released(gate_released);
    TEST_ASSERT_EQUAL_INT(BRIDGE_HTTP_OK,
                          bridge_http_get(&origin, BRIDGE_EVENT_GET_PATH, 5000, 5000, body, sizeof(body)).result);
    TEST_ASSERT_EQUAL_UINT(1, gate_releases);
    finite_busy = true;
    TEST_ASSERT_EQUAL_INT(BRIDGE_HTTP_BUSY,
                          bridge_http_get(&origin, BRIDGE_EVENT_GET_PATH, 5000, 5000, body, sizeof(body)).result);
    TEST_ASSERT_EQUAL_UINT(1, gate_releases);
}

static void clock_and_gate_fail_before_dispatch(void)
{
    char body[64];
    memset(body, 'x', sizeof(body));
    TEST_ASSERT_EQUAL_INT(BRIDGE_HTTP_INVALID_ARGUMENT, bridge_http_get(NULL, "/v1/device/events", 1, 5000, body, sizeof(body)).result);
    TEST_ASSERT_EQUAL_INT(0, body[63]);
    memset(body, 'x', sizeof(body));
    clock_ready = false;
    TEST_ASSERT_EQUAL_INT(BRIDGE_HTTP_CLOCK_NOT_READY, bridge_http_get(&origin, "/v1/device/events", 1, 5000, body, sizeof(body)).result);
    TEST_ASSERT_EQUAL_INT(0, body[63]);
    clock_ready = true; finite_busy = true;
    TEST_ASSERT_EQUAL_INT(BRIDGE_HTTP_BUSY, bridge_http_get(&origin, "/v1/device/events", 1, 5000, body, sizeof(body)).result);
    TEST_ASSERT_EQUAL_UINT(0, dispatches);
}

static void allocation_and_setup_failures_release_resources_and_gates(void)
{
    char body[64];
    fail_transport_call = 1;
    TEST_ASSERT_EQUAL_INT(BRIDGE_HTTP_NO_MEMORY, bridge_http_get(&origin, "/v1/device/events", 1, 5000, body, sizeof(body)).result);
    fail_transport_call = 2;
    TEST_ASSERT_EQUAL_INT(BRIDGE_HTTP_NO_MEMORY, bridge_http_get(&origin, "/v1/device/events", 1, 5000, body, sizeof(body)).result);
    fail_transport_call = 0; fail_client = true;
    TEST_ASSERT_EQUAL_INT(BRIDGE_HTTP_NO_MEMORY, bridge_http_get(&origin, "/v1/device/events", 1, 5000, body, sizeof(body)).result);
    fail_client = false; fail_header = true;
    TEST_ASSERT_EQUAL_INT(BRIDGE_HTTP_NO_MEMORY, bridge_http_get(&origin, "/v1/device/events", 1, 5000, body, sizeof(body)).result);
    fail_header = false; handshake_busy = true;
    TEST_ASSERT_EQUAL_INT(BRIDGE_HTTP_TIMEOUT, bridge_http_get(&origin, "/v1/device/events", 1, 5000, body, sizeof(body)).result);
    TEST_ASSERT_EQUAL_UINT(0, dispatches);
    handshake_busy = false;
    TEST_ASSERT_EQUAL_INT(BRIDGE_HTTP_OK, bridge_http_get(&origin, "/v1/device/events", 1, 5000, body, sizeof(body)).result);
    TEST_ASSERT_EQUAL_UINT(1, dispatches);
}

static void errors_are_bounded_and_never_redispatched(void)
{
    char body[64];
    mode = DENIED;
    bridge_http_response_t denied = bridge_http_get(&origin, "/v1/device/events", 1, 5000, body, sizeof(body));
    TEST_ASSERT_EQUAL_INT(BRIDGE_HTTP_OK, denied.result); TEST_ASSERT_EQUAL_INT(401, denied.status);
    TEST_ASSERT_EQUAL_UINT(1, dispatches);
    mode = TRUNCATED;
    TEST_ASSERT_EQUAL_INT(BRIDGE_HTTP_INCOMPLETE, bridge_http_get(&origin, "/v1/device/events", 1, 5000, body, sizeof(body)).result);
    TEST_ASSERT_EQUAL_INT(0, body[0]);
    mode = RESPONSE_OVERFLOW;
    TEST_ASSERT_EQUAL_INT(BRIDGE_HTTP_TOO_LARGE, bridge_http_get(&origin, "/v1/device/events", 1, 5000, body, 4).result);
    mode = BAD_ENCODING;
    TEST_ASSERT_EQUAL_INT(BRIDGE_HTTP_INVALID_RESPONSE, bridge_http_get(&origin, "/v1/device/events", 1, 5000, body, sizeof(body)).result);
    mode = LONG_HEADERS;
    TEST_ASSERT_EQUAL_INT(BRIDGE_HTTP_TOO_LARGE, bridge_http_get(&origin, "/v1/device/events", 1, 5000, body, sizeof(body)).result);
    mode = FALSE_EOF;
    TEST_ASSERT_EQUAL_INT(BRIDGE_HTTP_TOO_LARGE, bridge_http_get(&origin, "/v1/device/events", 1, 5000, body, sizeof(body)).result);
    TEST_ASSERT_EQUAL_UINT(6, dispatches);
}

static void deadlines_bound_stalls_and_free_gates(void)
{
    char body[64];
    mode = CONNECT_STALL;
    TEST_ASSERT_EQUAL_INT(BRIDGE_HTTP_TIMEOUT, bridge_http_get(&origin, "/v1/device/events", 1, 20, body, sizeof(body)).result);
    now_us = 0; mode = SLOW;
    TEST_ASSERT_EQUAL_INT(BRIDGE_HTTP_TIMEOUT, bridge_http_get(&origin, "/v1/device/events", 1, 1000, body, sizeof(body)).result);
    now_us = 0; mode = DEADLINE_CLOSE;
    TEST_ASSERT_EQUAL_INT(BRIDGE_HTTP_TIMEOUT, bridge_http_get(&origin, "/v1/device/events", 1, 1000, body, sizeof(body)).result);
    TEST_ASSERT_EQUAL_INT(0, body[0]);
    now_us = 0; mode = NORMAL;
    TEST_ASSERT_EQUAL_INT(BRIDGE_HTTP_OK, bridge_http_get(&origin, "/v1/device/events", 1, 5000, body, sizeof(body)).result);
    TEST_ASSERT_EQUAL_UINT(4, dispatches);
}

/* Each byte holds its offset, so writes can check they receive the body in order. */
static bool read_source(size_t offset, void *buffer, size_t capacity, void *context)
{
    (void)context;
    if (fail_source_at != 0U && ++source_reads == fail_source_at) return false;
    for (size_t i = 0; i < capacity; ++i) ((uint8_t *)buffer)[i] = (uint8_t)(offset + i);
    return true;
}

static bridge_http_mutation_t put(void)
{
    static char body[64];
    return bridge_http_put_wav(&origin, "/v1/device/assistant-requests/12345678-1234-4abc-9876-123456789abc",
                               UPLOAD_BYTES, read_source, NULL, body, sizeof(body));
}

static void upload_sends_the_whole_body_once_and_reads_the_response(void)
{
    mode = PUT_OK;
    bridge_http_mutation_t upload = put();
    TEST_ASSERT_EQUAL_INT(BRIDGE_HTTP_OK, upload.response.result);
    TEST_ASSERT_EQUAL_INT(202, upload.response.status);
    TEST_ASSERT_EQUAL_UINT(strlen(payload), upload.response.length);
    TEST_ASSERT_FALSE(upload.unsent);
    TEST_ASSERT_EQUAL_UINT(UPLOAD_BYTES, uploaded);
    TEST_ASSERT_TRUE(nodelay_set);
    TEST_ASSERT_EQUAL_UINT(1, dispatches);
    TEST_ASSERT_EQUAL_STRING("https://example.test/prefix/v1/device/assistant-requests/12345678-1234-4abc-9876-123456789abc",
                             captured_url);
}

static void upload_is_unsent_only_before_the_last_block(void)
{
    mode = PUT_FAIL_FIRST_WRITE;
    bridge_http_mutation_t upload = put();
    TEST_ASSERT_EQUAL_INT(BRIDGE_HTTP_TRANSPORT_ERROR, upload.response.result);
    TEST_ASSERT_TRUE(upload.unsent);
    mode = PUT_OK; fail_source_at = 3;
    upload = put();
    TEST_ASSERT_EQUAL_INT(BRIDGE_HTTP_SOURCE_FAILED, upload.response.result);
    TEST_ASSERT_TRUE(upload.unsent);
    uploaded = 0; fail_source_at = 0; mode = PUT_FAIL_LAST_WRITE;
    upload = put();
    TEST_ASSERT_EQUAL_INT(BRIDGE_HTTP_TRANSPORT_ERROR, upload.response.result);
    TEST_ASSERT_FALSE(upload.unsent);
    TEST_ASSERT_EQUAL_UINT(UPLOAD_BYTES - UPLOAD_BYTES % BRIDGE_ASSISTANT_UPLOAD_BLOCK_BYTES, uploaded);
    TEST_ASSERT_EQUAL_UINT(3, dispatches);
}

static void upload_respects_gate_size_and_progress_window(void)
{
    char body[64];
    finite_busy = true;
    bridge_http_mutation_t upload = put();
    TEST_ASSERT_EQUAL_INT(BRIDGE_HTTP_BUSY, upload.response.result);
    TEST_ASSERT_TRUE(upload.unsent);
    finite_busy = false;
    upload = bridge_http_put_wav(&origin, "/v1/device/assistant-requests/x", BRIDGE_ASSISTANT_WAV_MIN_BYTES - 1U,
                                 read_source, NULL, body, sizeof(body));
    TEST_ASSERT_EQUAL_INT(BRIDGE_HTTP_INVALID_ARGUMENT, upload.response.result);
    TEST_ASSERT_EQUAL_UINT(0, dispatches);
    mode = PUT_STALL;
    upload = put();
    TEST_ASSERT_EQUAL_INT(BRIDGE_HTTP_TIMEOUT, upload.response.result);
    TEST_ASSERT_TRUE(upload.unsent);
    TEST_ASSERT_EQUAL_UINT(1, dispatches);
}

/* A slow open does not consume the progress cap, and renewals stop at it. */
static void upload_cap_starts_after_opening(void)
{
    mode = PUT_SLOW;
    bridge_http_mutation_t upload = put();
    TEST_ASSERT_EQUAL_INT(BRIDGE_HTTP_TIMEOUT, upload.response.result);
    TEST_ASSERT_EQUAL_UINT(BRIDGE_ASSISTANT_UPLOAD_BLOCK_BYTES / 2U, uploaded);
}

typedef struct { bool opened; unsigned checks; } stream_test_t;
static bool stream_opened(void *context) { ((stream_test_t *)context)->opened = true; return true; }
static bool stream_open_rejected(void *context) { (void)context; return false; }
static bool stream_current(void *context) { return ++((stream_test_t *)context)->checks < 3U; }
static bool stream_always_current(void *context) { (void)context; return true; }

static void stream_opens_with_auth_processes_frame_and_releases_resources(void)
{
    bridge_sse_t state;
    bridge_sse_init(&state);
    stream_test_t context = {0};
    bridge_stream_hooks_t hooks = {.opened = stream_opened, .current = stream_current, .context = &context};
    mode = STREAM_OK;
    bridge_stream_result_t result = bridge_stream_run(&origin, &state, &hooks);
    TEST_ASSERT_EQUAL_INT(BRIDGE_STREAM_STOPPED, result.reason);
    TEST_ASSERT_TRUE(context.opened);
    TEST_ASSERT_TRUE(state.connected);
    TEST_ASSERT_TRUE(bridge_sse_take_pending_events(&state));
    TEST_ASSERT_EQUAL_STRING("https://example.test/prefix/v1/device/event-stream", captured_url);
    TEST_ASSERT_TRUE(auth_set);
    TEST_ASSERT_EQUAL_UINT(1, dispatches);
}

static void stream_rejects_invalid_headers_and_unauthorized_response(void)
{
    bridge_sse_t state;
    bridge_sse_init(&state);
    stream_test_t context = {0};
    bridge_stream_hooks_t hooks = {.opened = stream_opened, .current = stream_current, .context = &context};
    mode = STREAM_NO_TYPE;
    bridge_stream_result_t result = bridge_stream_run(&origin, &state, &hooks);
    TEST_ASSERT_EQUAL_INT(BRIDGE_STREAM_INVALID_HEADERS, result.reason);
    TEST_ASSERT_FALSE(context.opened);
    mode = STREAM_DENIED;
    result = bridge_stream_run(&origin, &state, &hooks);
    TEST_ASSERT_EQUAL_INT(BRIDGE_STREAM_HTTP_REJECTED, result.reason);
    TEST_ASSERT_EQUAL_INT(401, result.http_status);
    TEST_ASSERT_FALSE(context.opened);
    mode = STREAM_ENCODED;
    TEST_ASSERT_EQUAL_INT(BRIDGE_STREAM_INVALID_HEADERS, bridge_stream_run(&origin, &state, &hooks).reason);
    mode = STREAM_TWO_TYPES;
    TEST_ASSERT_EQUAL_INT(BRIDGE_STREAM_INVALID_HEADERS, bridge_stream_run(&origin, &state, &hooks).reason);
    TEST_ASSERT_FALSE(context.opened);
    TEST_ASSERT_EQUAL_UINT(4, dispatches);
}

static void stream_idle_times_out_after_sixty_seconds(void)
{
    bridge_sse_t state;
    bridge_sse_init(&state);
    stream_test_t context = {0};
    bridge_stream_hooks_t hooks = {.opened = stream_opened, .current = stream_always_current, .context = &context};
    mode = STREAM_IDLE;
    bridge_stream_result_t result = bridge_stream_run(&origin, &state, &hooks);
    TEST_ASSERT_EQUAL_INT(BRIDGE_STREAM_HEARTBEAT_TIMEOUT, result.reason);
    TEST_ASSERT_TRUE(context.opened);
    TEST_ASSERT_GREATER_OR_EQUAL_INT64(INT64_C(60000000), now_us);
    TEST_ASSERT_LESS_THAN_INT64(INT64_C(61000000), now_us);
}

static void stream_reports_connection_failure(void)
{
    bridge_sse_t state;
    bridge_sse_init(&state);
    stream_test_t context = {0};
    bridge_stream_hooks_t hooks = {.opened = stream_opened, .current = stream_current, .context = &context};
    mode = CONNECT_STALL;
    bridge_stream_result_t result = bridge_stream_run(&origin, &state, &hooks);
    TEST_ASSERT_EQUAL_INT(BRIDGE_STREAM_CONNECT_FAILED, result.reason);
    TEST_ASSERT_EQUAL_INT(0, result.http_status);
    TEST_ASSERT_FALSE(context.opened);
}

static void stream_stale_open_does_not_publish_connected(void)
{
    bridge_sse_t state;
    bridge_sse_init(&state);
    stream_test_t context = {0};
    bridge_stream_hooks_t hooks = {.opened = stream_open_rejected, .current = stream_current, .context = &context};
    mode = STREAM_OK;
    bridge_stream_result_t result = bridge_stream_run(&origin, &state, &hooks);
    TEST_ASSERT_EQUAL_INT(BRIDGE_STREAM_STOPPED, result.reason);
    TEST_ASSERT_FALSE(state.connected);
    TEST_ASSERT_EQUAL_UINT(0, context.checks);
}

int main(void)
{
    UNITY_BEGIN();
    TEST_ASSERT_EQUAL_INT(ESP_OK, bridge_http_init());
    RUN_TEST(complete_get_with_prefix_and_auth);
    RUN_TEST(ack_json_is_one_put_with_empty_204);
    RUN_TEST(snooze_json_is_one_post_with_bounded_response);
    RUN_TEST(actions_send_no_body_or_content_type);
    RUN_TEST(mutations_are_unsent_only_before_the_request_is_written);
    RUN_TEST(finite_gate_release_wakes_a_waiting_owner);
    RUN_TEST(clock_and_gate_fail_before_dispatch);
    RUN_TEST(allocation_and_setup_failures_release_resources_and_gates);
    RUN_TEST(errors_are_bounded_and_never_redispatched);
    RUN_TEST(deadlines_bound_stalls_and_free_gates);
    RUN_TEST(upload_sends_the_whole_body_once_and_reads_the_response);
    RUN_TEST(upload_is_unsent_only_before_the_last_block);
    RUN_TEST(upload_respects_gate_size_and_progress_window);
    RUN_TEST(upload_cap_starts_after_opening);
    RUN_TEST(stream_opens_with_auth_processes_frame_and_releases_resources);
    RUN_TEST(stream_rejects_invalid_headers_and_unauthorized_response);
    RUN_TEST(stream_idle_times_out_after_sixty_seconds);
    RUN_TEST(stream_reports_connection_failure);
    RUN_TEST(stream_stale_open_does_not_publish_connected);
    return UNITY_END();
}
