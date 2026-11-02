#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "unity.h"
#include "notifications.h"
#include "notification_journal.h"
#include "bridge_http.h"
#include "bridge_origin_store.h"
#include "../../components/features/notifications/src/notifications.c"

static notification_state_t durable;
static unsigned saves, get_calls, acks, holds, releases, publications;
static bool fail_ack, busy_get, fail_save;
static void (*gate_callback)(void);

typedef struct { close_command_t item; bool has_item; } fake_queue_t;

QueueHandle_t xQueueCreate(unsigned length, unsigned size)
{
    (void)length; (void)size;
    return calloc(1, sizeof(fake_queue_t));
}
void vQueueDelete(QueueHandle_t queue) { free(queue); }
BaseType_t xQueueSend(QueueHandle_t queue, const void *item, TickType_t wait)
{
    (void)wait;
    fake_queue_t *fake = queue;
    if (fake->has_item) return 0;
    fake->item = *(const close_command_t *)item;
    fake->has_item = true;
    return pdTRUE;
}
BaseType_t xQueueReceive(QueueHandle_t queue, void *item, TickType_t wait)
{
    (void)wait;
    fake_queue_t *fake = queue;
    if (!fake->has_item) return 0;
    *(close_command_t *)item = fake->item;
    fake->has_item = false;
    return pdTRUE;
}
unsigned uxQueueMessagesWaiting(QueueHandle_t queue) { return ((fake_queue_t *)queue)->has_item ? 1U : 0U; }
BaseType_t xTaskCreate(void (*task)(void *), const char *name, unsigned stack, void *arg,
                       unsigned priority, TaskHandle_t *handle)
{
    (void)task; (void)name; (void)stack; (void)arg; (void)priority;
    *handle = (void *)1;
    return pdPASS;
}
void xTaskNotifyGive(TaskHandle_t task) { (void)task; }
unsigned ulTaskNotifyTake(BaseType_t clear, TickType_t timeout) { (void)clear; (void)timeout; return 1; }
void vTaskDelete(TaskHandle_t task) { (void)task; }
esp_err_t esp_timer_create(const esp_timer_create_args_t *args, esp_timer_handle_t *timer)
{ (void)args; *timer = (void *)1; return ESP_OK; }
esp_err_t esp_timer_stop(esp_timer_handle_t timer) { (void)timer; return ESP_OK; }
esp_err_t esp_timer_delete(esp_timer_handle_t timer) { (void)timer; return ESP_OK; }
esp_err_t esp_timer_start_once(esp_timer_handle_t timer, uint64_t micros)
{ (void)timer; TEST_ASSERT_EQUAL_UINT64(UINT64_C(5000000), micros); return ESP_OK; }
const char *esp_err_to_name(esp_err_t error) { (void)error; return "fake"; }

void bridge_http_set_gate_released(void (*callback)(void)) { gate_callback = callback; }
bool bridge_origin_hold(void) { ++holds; return true; }
void bridge_origin_release(void) { ++releases; }
esp_err_t bridge_origin_load(bridge_origin_t *origin)
{
    memset(origin, 0, sizeof(*origin));
    strcpy(origin->url, "https://example.test");
    strcpy(origin->token, "01234567890123456789012345678901");
    return ESP_OK;
}
esp_err_t notification_journal_load(notification_state_t *loaded) { *loaded = durable; return ESP_OK; }
esp_err_t notification_journal_save(const notification_state_t *next)
{
    if (fail_save) return ESP_FAIL;
    durable = *next;
    ++saves;
    return ESP_OK;
}
bool bridge_event_valid(const bridge_event_t *event) { return event != NULL && bridge_uuid_valid(event->event_id); }
bridge_event_decode_t bridge_event_decode(const char *body, size_t length, int status,
                                          bridge_event_t *event, bool *present)
{
    (void)body; (void)length;
    TEST_ASSERT_EQUAL_INT(200, status);
    *present = get_calls == 1U;
    if (*present) {
        memset(event, 0, sizeof(*event));
        strcpy(event->event_id, "01234567-89ab-cdef-0123-456789abcdef");
        event->alert_id = 42;
        strcpy(event->message, "Reminder");
        strcpy(event->scheduled_at, "2026-11-02T12:00:00Z");
        strcpy(event->triggered_at, "2026-11-02T12:00:01Z");
    }
    return BRIDGE_EVENT_DECODE_OK;
}
bool bridge_event_ack_succeeded(int status, size_t length) { return status == 204 && length == 0U; }
bridge_http_response_t bridge_http_get(const bridge_origin_t *origin, const char *path, uint32_t gate_ms,
                                       uint32_t io_ms, char *body, size_t capacity)
{
    (void)origin; (void)gate_ms; (void)io_ms; (void)capacity;
    TEST_ASSERT_EQUAL_STRING(BRIDGE_EVENT_GET_PATH, path);
    if (busy_get) { busy_get = false; return (bridge_http_response_t){.result = BRIDGE_HTTP_BUSY}; }
    strcpy(body, "{}");
    ++get_calls;
    if (gate_callback != NULL) gate_callback();
    return (bridge_http_response_t){.result = BRIDGE_HTTP_OK, .status = 200, .length = 2};
}
bridge_http_response_t bridge_http_json(const bridge_origin_t *origin, const char *path,
                                        bridge_http_json_method_t method, const char *request, size_t request_length,
                                        uint32_t gate_ms, uint32_t io_ms, char *body, size_t capacity)
{
    (void)origin; (void)gate_ms; (void)io_ms; (void)body; (void)capacity;
    TEST_ASSERT_EQUAL_INT(BRIDGE_HTTP_PUT_JSON, method);
    TEST_ASSERT_EQUAL_STRING(BRIDGE_EVENT_ACK_BODY, request);
    TEST_ASSERT_EQUAL_UINT(strlen(BRIDGE_EVENT_ACK_BODY), request_length);
    TEST_ASSERT_TRUE(strstr(path, durable.pending_ack) != NULL);
    TEST_ASSERT_GREATER_THAN(0, saves);
    ++acks;
    if (gate_callback != NULL) gate_callback();
    return fail_ack ? (bridge_http_response_t){.result = BRIDGE_HTTP_TIMEOUT} :
                      (bridge_http_response_t){.result = BRIDGE_HTTP_OK, .status = 204, .length = 0};
}

static void show_view(const notification_view_t *view)
{
    ++publications;
    TEST_ASSERT_EQUAL_UINT(durable.count, view->count);
}

void setUp(void)
{
    if (commands != NULL) vQueueDelete(commands);
    commands = NULL; retry_timer = NULL; worker = NULL; starting = drain_requested = waiting_gate = false;
    stream_online = origin_held = uncertain = false;
    gate_epoch = 0; generation = arrival_sequence = 0; notice = NOTIFICATION_VIEW_OK;
    view_callback = NULL; gate_callback = NULL;
    notification_state_init(&durable);
    saves = get_calls = acks = holds = releases = publications = 0;
    fail_ack = busy_get = fail_save = false;
}
void tearDown(void) {}

static void receipt_is_saved_before_ack_and_close_is_local(void)
{
    TEST_ASSERT_EQUAL_INT(ESP_OK, notifications_start(show_view));
    notifications_bridge_online(true);
    run(NULL);
    TEST_ASSERT_EQUAL_UINT(1, durable.count);
    TEST_ASSERT_EQUAL_STRING("", durable.pending_ack);
    TEST_ASSERT_EQUAL_UINT(2, saves);
    TEST_ASSERT_EQUAL_UINT(1, acks);
    TEST_ASSERT_EQUAL_UINT(2, get_calls);
    TEST_ASSERT_EQUAL_UINT(1, holds);
    notifications_close(generation, durable.cards[0].event_id);
    run(NULL);
    TEST_ASSERT_EQUAL_UINT(0, durable.count);
    TEST_ASSERT_EQUAL_UINT(1, acks);
    TEST_ASSERT_EQUAL_UINT(2, releases);
}

static void lost_ack_response_keeps_id_until_retry(void)
{
    TEST_ASSERT_EQUAL_INT(ESP_OK, notifications_start(show_view));
    fail_ack = true;
    notifications_bridge_online(true);
    run(NULL);
    TEST_ASSERT_EQUAL_UINT(1, durable.count);
    TEST_ASSERT_EQUAL_STRING(durable.cards[0].event_id, durable.pending_ack);
    TEST_ASSERT_EQUAL_UINT(1, saves);
    fail_ack = false;
    retry_timer_fired(NULL);
    run(NULL);
    TEST_ASSERT_EQUAL_STRING("", durable.pending_ack);
    TEST_ASSERT_EQUAL_UINT(2, acks);
}

static void busy_get_resumes_on_gate_release(void)
{
    TEST_ASSERT_EQUAL_INT(ESP_OK, notifications_start(show_view));
    busy_get = true;
    notifications_bridge_online(true);
    run(NULL);
    TEST_ASSERT_EQUAL_UINT(0, get_calls);
    TEST_ASSERT_TRUE(waiting_gate);
    notifications_gate_released();
    run(NULL);
    TEST_ASSERT_EQUAL_UINT(1, durable.count);
    TEST_ASSERT_EQUAL_UINT(1, acks);
}

static void failed_journal_commit_blocks_ack_and_keeps_event(void)
{
    TEST_ASSERT_EQUAL_INT(ESP_OK, notifications_start(show_view));
    fail_save = true;
    notifications_bridge_online(true);
    run(NULL);
    TEST_ASSERT_EQUAL_UINT(0, durable.count);
    TEST_ASSERT_EQUAL_UINT(0, acks);
    TEST_ASSERT_TRUE(uncertain);
    TEST_ASSERT_EQUAL_INT(NOTIFICATION_VIEW_STORAGE_ERROR, notice);
    TEST_ASSERT_EQUAL_UINT(0, releases);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(receipt_is_saved_before_ack_and_close_is_local);
    RUN_TEST(lost_ack_response_keeps_id_until_retry);
    RUN_TEST(busy_get_resumes_on_gate_release);
    RUN_TEST(failed_journal_commit_blocks_ack_and_keeps_event);
    return UNITY_END();
}
