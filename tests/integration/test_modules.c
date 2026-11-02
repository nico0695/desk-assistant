#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bridge_http.h"
#include "bridge_origin_store.h"
#include "modules.h"
#include "recording.h"
#include "unity.h"
#include "../../components/features/modules/src/modules.c"

void setUp(void) {}
void tearDown(void) {}

#define T0 "2026-11-02T12:00:00Z"

static int mutex_storage;
static int held;
static bool create_fails;
static bool audio_blocked;
static bool origin_missing;
static unsigned get_calls;
static char last_path[128];
static bridge_http_response_t next_response;
static const char *next_body;
static modules_view_t views[32];
static unsigned view_count;
static bool defer_workers;
static bool action_slot_taken;
static unsigned actions_sent, action_slot_releases;
static int last_method;
static uint32_t last_action_io_ms;
static char last_request[160];
static bridge_http_mutation_t next_action;
static const char *next_action_body;
static void (*deferred_task)(void *);

SemaphoreHandle_t xSemaphoreCreateMutex(void) { return &mutex_storage; }
BaseType_t xSemaphoreTake(SemaphoreHandle_t semaphore, TickType_t wait)
{
    (void)semaphore; (void)wait;
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, held, "the lock is not recursive");
    ++held;
    return pdTRUE;
}
BaseType_t xSemaphoreGive(SemaphoreHandle_t semaphore) { (void)semaphore; --held; return pdTRUE; }
BaseType_t xTaskCreate(void (*task)(void *), const char *name, unsigned stack, void *arg, unsigned priority,
                       TaskHandle_t *handle)
{
    (void)name; (void)stack; (void)priority; (void)handle;
    TEST_ASSERT_EQUAL_INT(0, held);
    if (create_fails) return 0;
    if (defer_workers) {
        deferred_task = task;
        return pdPASS;
    }
    task(arg);
    return pdPASS;
}
void vTaskDelete(TaskHandle_t task) { (void)task; }
void memory_report_log(const char *checkpoint) { (void)checkpoint; }
bool recording_reads_blocked(void) { return audio_blocked; }
esp_err_t bridge_origin_load(bridge_origin_t *origin)
{
    memset(origin, 0, sizeof(*origin));
    return origin_missing ? ESP_FAIL : ESP_OK;
}
bridge_http_response_t bridge_http_get(const bridge_origin_t *origin, const char *path, uint32_t gate_ms,
                                       uint32_t io_ms, char *body, size_t capacity)
{
    (void)origin;
    TEST_ASSERT_EQUAL_INT(0, held);
    TEST_ASSERT_EQUAL_UINT32(BRIDGE_RESOURCE_GATE_WAIT_MS, gate_ms);
    TEST_ASSERT_EQUAL_UINT32(BRIDGE_RESOURCE_IO_MS, io_ms);
    TEST_ASSERT_EQUAL_size_t(BRIDGE_RESOURCE_BODY_MAX_BYTES, capacity);
    ++get_calls;
    snprintf(last_path, sizeof(last_path), "%s", path);
    if (next_response.result == BRIDGE_HTTP_OK) {
        memcpy(body, next_body, strlen(next_body));
        next_response.length = strlen(next_body);
    }
    return next_response;
}

bool recording_external_action_begin(void) { return !action_slot_taken; }
void recording_external_action_end(void) { ++action_slot_releases; }

static bridge_http_mutation_t answer_action(const char *path, uint32_t gate_ms, uint32_t io_ms, char *body,
                                            size_t capacity)
{
    TEST_ASSERT_EQUAL_INT(0, held);
    TEST_ASSERT_EQUAL_UINT32(BRIDGE_RESOURCE_ACTION_GATE_WAIT_MS, gate_ms);
    TEST_ASSERT_EQUAL_size_t(BRIDGE_RESOURCE_BODY_MAX_BYTES, capacity);
    ++actions_sent;
    last_action_io_ms = io_ms;
    snprintf(last_path, sizeof(last_path), "%s", path);
    bridge_http_mutation_t result = next_action;
    if (result.response.result == BRIDGE_HTTP_OK) {
        memcpy(body, next_action_body, strlen(next_action_body));
        result.response.length = strlen(next_action_body);
    }
    return result;
}

bridge_http_mutation_t bridge_http_action(const bridge_origin_t *origin, const char *path,
                                          bridge_http_action_method_t method, uint32_t gate_ms, uint32_t io_ms,
                                          char *body, size_t capacity)
{
    (void)origin;
    last_method = (int)method;
    last_request[0] = '\0';
    return answer_action(path, gate_ms, io_ms, body, capacity);
}

bridge_http_mutation_t bridge_http_json(const bridge_origin_t *origin, const char *path,
                                        bridge_http_json_method_t method, const char *request, size_t length,
                                        uint32_t gate_ms, uint32_t io_ms, char *body, size_t capacity)
{
    (void)origin;
    TEST_ASSERT_EQUAL_INT(BRIDGE_HTTP_POST_JSON, method);
    last_method = -1;
    snprintf(last_request, sizeof(last_request), "%.*s", (int)length, request);
    return answer_action(path, gate_ms, io_ms, body, capacity);
}

static void on_view(const modules_view_t *view)
{
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, held, "views are published outside the lock");
    views[view_count++ % 32U] = *view;
}

static const modules_view_t *latest(void) { return &views[(view_count - 1U) % 32U]; }

static void reply(int status, const char *body)
{
    next_response = (bridge_http_response_t){.result = BRIDGE_HTTP_OK, .status = status};
    next_body = body;
}

static const char *NOTES_PAGE =
    "{\"items\":[{\"id\":11,\"title\":\"One\",\"created_at\":\"" T0 "\"},{\"id\":12,\"title\":\"Two\",\"created_at\":\"" T0 "\"}],"
    "\"page\":1,\"page_size\":5,\"has_previous\":false,\"has_next\":false}";
static const char *ALERTS_PAGE =
    "{\"items\":[{\"id\":7,\"message\":\"Call\",\"scheduled_at\":\"" T0 "\",\"created_at\":\"" T0 "\"}],"
    "\"page\":1,\"page_size\":5,\"has_previous\":false,\"has_next\":false}";
static const char *NOTE_DETAIL =
    "{\"item\":{\"id\":12,\"title\":\"Two\",\"description\":\"Body\",\"tag\":null,\"channel_id\":null,"
    "\"created_at\":\"" T0 "\",\"updated_at\":\"" T0 "\"}}";

static const char *TASKS_PAGE =
    "{\"items\":[{\"id\":5,\"title\":\"Buy\",\"status\":\"pending\",\"alert_at\":null,\"created_at\":\"" T0 "\"}],"
    "\"page\":1,\"page_size\":5,\"has_previous\":false,\"has_next\":false}";
static const char *TASK_DETAIL =
    "{\"item\":{\"id\":5,\"title\":\"Buy\",\"description\":\"\",\"tag\":null,\"channel_id\":null,"
    "\"status\":\"pending\",\"alert_at\":null,\"created_at\":\"" T0 "\"}}";
static const char *ALERT_DETAIL =
    "{\"item\":{\"id\":7,\"message\":\"Call\",\"scheduled_at\":\"" T0 "\",\"sent\":false,"
    "\"channel_id\":null,\"created_at\":\"" T0 "\"}}";

static void reset(void)
{
    modules_leave();
    free(list_slot);
    free(detail_slot);
    list_slot = NULL;
    detail_slot = NULL;
    memset(&state, 0, sizeof(state));
    held = 0;
    create_fails = audio_blocked = origin_missing = defer_workers = action_slot_taken = false;
    actions_sent = action_slot_releases = 0U;
    get_calls = 0U;
    view_count = 0U;
    deferred_task = NULL;
}

static void copy_first_entry(const bridge_resource_page_t *page, void *context)
{
    *(uint64_t *)context = page->entries[0].id;
}

static void copy_title(const bridge_resource_detail_t *detail, void *context)
{
    snprintf((char *)context, 16U, "%s", detail->text);
}

static void entering_reads_the_first_page_and_exposes_it_to_the_reader(void)
{
    reset();
    reply(200, NOTES_PAGE);
    modules_enter(BRIDGE_RESOURCE_NOTES);
    TEST_ASSERT_EQUAL_STRING("/v1/device/notes?page=1", last_path);
    TEST_ASSERT_EQUAL_UINT(2U, view_count);
    TEST_ASSERT_EQUAL(MODULES_LOADING, views[0].status);
    TEST_ASSERT_EQUAL(MODULES_READY, latest()->status);
    TEST_ASSERT_EQUAL_UINT8(2U, latest()->count);
    TEST_ASSERT_GREATER_THAN_UINT64(views[0].revision, latest()->revision);
    uint64_t first = 0U;
    TEST_ASSERT_TRUE(modules_read_page(latest()->ticket, copy_first_entry, &first));
    TEST_ASSERT_EQUAL_UINT64(11U, first);
    TEST_ASSERT_FALSE(modules_read_page(latest()->ticket + 1U, copy_first_entry, &first));
    TEST_ASSERT_FALSE(modules_state_is_current(&state, 0U));
}

static void a_detail_is_read_by_id_and_back_keeps_the_list_without_a_request(void)
{
    reset();
    reply(200, NOTES_PAGE);
    modules_enter(BRIDGE_RESOURCE_NOTES);
    reply(200, NOTE_DETAIL);
    modules_open_detail(latest()->ticket, 12U);
    TEST_ASSERT_EQUAL_STRING("/v1/device/notes/12", last_path);
    TEST_ASSERT_EQUAL(MODULES_SCREEN_DETAIL, latest()->screen);
    TEST_ASSERT_EQUAL(MODULES_READY, latest()->status);
    char title[16];
    TEST_ASSERT_TRUE(modules_read_detail(latest()->ticket, copy_title, title));
    TEST_ASSERT_EQUAL_STRING("Two", title);
    TEST_ASSERT_NOT_NULL(list_slot);
    const unsigned calls = get_calls;
    modules_back(latest()->ticket);
    TEST_ASSERT_EQUAL_UINT(calls, get_calls);
    TEST_ASSERT_EQUAL(MODULES_SCREEN_LIST, latest()->screen);
    TEST_ASSERT_EQUAL(MODULES_READY, latest()->status);
    TEST_ASSERT_NULL(detail_slot);
    uint64_t first = 0U;
    TEST_ASSERT_TRUE(modules_read_page(latest()->ticket, copy_first_entry, &first));
    TEST_ASSERT_FALSE(modules_read_detail(latest()->ticket, copy_title, title));
}

static void audio_in_use_prevents_the_request_and_refresh_retries(void)
{
    reset();
    audio_blocked = true;
    modules_enter(BRIDGE_RESOURCE_NOTES);
    TEST_ASSERT_EQUAL_UINT(0U, get_calls);
    TEST_ASSERT_EQUAL(MODULES_FAILED, latest()->status);
    TEST_ASSERT_EQUAL(MODULES_ERROR_BUSY_AUDIO, latest()->error);
    audio_blocked = false;
    reply(200, NOTES_PAGE);
    modules_refresh(latest()->ticket);
    TEST_ASSERT_EQUAL_UINT(1U, get_calls);
    TEST_ASSERT_EQUAL(MODULES_READY, latest()->status);
}

static void transport_and_codec_failures_map_to_distinct_errors(void)
{
    static const struct {
        bridge_http_result_t result;
        modules_error_t error;
    } cases[] = {{BRIDGE_HTTP_BUSY, MODULES_ERROR_BUSY_BRIDGE}, {BRIDGE_HTTP_TIMEOUT, MODULES_ERROR_NETWORK},
                 {BRIDGE_HTTP_CLOCK_NOT_READY, MODULES_ERROR_NETWORK}, {BRIDGE_HTTP_TOO_LARGE, MODULES_ERROR_TOO_LARGE},
                 {BRIDGE_HTTP_INCOMPLETE, MODULES_ERROR_INVALID}, {BRIDGE_HTTP_NO_MEMORY, MODULES_ERROR_NO_MEMORY}};
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        reset();
        next_response = (bridge_http_response_t){.result = cases[i].result};
        modules_enter(BRIDGE_RESOURCE_NOTES);
        TEST_ASSERT_EQUAL(MODULES_FAILED, latest()->status);
        TEST_ASSERT_EQUAL_MESSAGE(cases[i].error, latest()->error, "transport mapping");
        TEST_ASSERT_NULL(list_slot);
    }
    reset();
    reply(404, "{\"error\":{\"code\":\"resource_not_found\",\"message\":\"x\"}}");
    modules_enter(BRIDGE_RESOURCE_NOTES);
    TEST_ASSERT_EQUAL(MODULES_ERROR_NOT_FOUND, latest()->error);
    reset();
    reply(200, "{\"items\":[]}");
    modules_enter(BRIDGE_RESOURCE_NOTES);
    TEST_ASSERT_EQUAL(MODULES_ERROR_INVALID, latest()->error);
    reset();
    origin_missing = true;
    modules_enter(BRIDGE_RESOURCE_NOTES);
    TEST_ASSERT_EQUAL(MODULES_ERROR_NETWORK, latest()->error);
    TEST_ASSERT_EQUAL_UINT(0U, get_calls);
}

static void a_result_that_arrives_after_leaving_is_freed_without_a_view(void)
{
    reset();
    defer_workers = true;
    reply(200, NOTES_PAGE);
    modules_enter(BRIDGE_RESOURCE_NOTES);
    TEST_ASSERT_NOT_NULL(deferred_task);
    const unsigned before = view_count;
    modules_leave();
    deferred_task(NULL);
    TEST_ASSERT_EQUAL_UINT(0U, get_calls);
    TEST_ASSERT_EQUAL_UINT(before, view_count);
    TEST_ASSERT_NULL(list_slot);
    TEST_ASSERT_FALSE(state.running);
}

static void the_newest_request_runs_after_the_one_in_flight(void)
{
    reset();
    defer_workers = true;
    reply(200, NOTES_PAGE);
    modules_enter(BRIDGE_RESOURCE_NOTES);
    modules_leave();
    reply(200, ALERTS_PAGE);
    modules_enter(BRIDGE_RESOURCE_ALERTS);
    TEST_ASSERT_EQUAL_UINT(0U, get_calls);
    deferred_task(NULL);
    /* The first request was stale and is skipped; the newest one is read by the same worker. */
    TEST_ASSERT_EQUAL_UINT(1U, get_calls);
    TEST_ASSERT_EQUAL_STRING("/v1/device/alerts?page=1", last_path);
    TEST_ASSERT_EQUAL(MODULES_READY, latest()->status);
    TEST_ASSERT_EQUAL(BRIDGE_RESOURCE_ALERTS, latest()->kind);
    TEST_ASSERT_FALSE(state.running);
}

static void a_worker_that_cannot_start_fails_the_view_and_frees_the_slot(void)
{
    reset();
    create_fails = true;
    modules_enter(BRIDGE_RESOURCE_NOTES);
    TEST_ASSERT_EQUAL(MODULES_FAILED, latest()->status);
    TEST_ASSERT_EQUAL(MODULES_ERROR_NO_MEMORY, latest()->error);
    TEST_ASSERT_FALSE(state.running);
    create_fails = false;
    reply(200, NOTES_PAGE);
    modules_refresh(latest()->ticket);
    TEST_ASSERT_EQUAL(MODULES_READY, latest()->status);
}

static void paging_replaces_the_list_and_keeps_one_slot(void)
{
    reset();
    static char first[1024], second[1024];
    snprintf(first, sizeof(first),
             "{\"items\":[{\"id\":1,\"title\":\"a\",\"created_at\":\"" T0 "\"},{\"id\":2,\"title\":\"b\",\"created_at\":\"" T0 "\"},"
             "{\"id\":3,\"title\":\"c\",\"created_at\":\"" T0 "\"},{\"id\":4,\"title\":\"d\",\"created_at\":\"" T0 "\"},"
             "{\"id\":5,\"title\":\"e\",\"created_at\":\"" T0 "\"}],\"page\":1,\"page_size\":5,\"has_previous\":false,\"has_next\":true}");
    snprintf(second, sizeof(second),
             "{\"items\":[{\"id\":6,\"title\":\"f\",\"created_at\":\"" T0 "\"}],\"page\":2,\"page_size\":5,"
             "\"has_previous\":true,\"has_next\":false}");
    reply(200, first);
    modules_enter(BRIDGE_RESOURCE_NOTES);
    TEST_ASSERT_TRUE(latest()->has_next);
    reply(200, second);
    modules_next(latest()->ticket);
    TEST_ASSERT_EQUAL_STRING("/v1/device/notes?page=2", last_path);
    TEST_ASSERT_EQUAL_UINT64(2U, latest()->page);
    uint64_t id = 0U;
    modules_read_page(latest()->ticket, copy_first_entry, &id);
    TEST_ASSERT_EQUAL_UINT64(6U, id);
    TEST_ASSERT_FALSE(latest()->has_next);
}

static void act_reply(int status, const char *body)
{
    next_action = (bridge_http_mutation_t){.response = {.result = BRIDGE_HTTP_OK, .status = status}};
    next_action_body = body;
}

/* Opens a module, then the detail of id with the given fixtures. */
static void open_detail(bridge_resource_kind_t kind, const char *page, uint64_t id, const char *detail)
{
    reset();
    reply(200, page);
    modules_enter(kind);
    reply(200, detail);
    modules_open_detail(latest()->ticket, id);
    TEST_ASSERT_EQUAL(MODULES_READY, latest()->status);
}

static void a_confirmed_delete_reads_a_fresh_origin_page(void)
{
    open_detail(BRIDGE_RESOURCE_NOTES, NOTES_PAGE, 12U, NOTE_DETAIL);
    TEST_ASSERT_TRUE(latest()->can_delete);
    reply(200, NOTES_PAGE);
    act_reply(200, "{\"id\":12,\"deleted\":true}");
    const unsigned reads = get_calls;
    modules_delete(latest()->ticket);
    TEST_ASSERT_EQUAL_UINT(1U, actions_sent);
    TEST_ASSERT_EQUAL_INT(BRIDGE_HTTP_DELETE, last_method);
    TEST_ASSERT_EQUAL_UINT32(BRIDGE_RESOURCE_IO_MS, last_action_io_ms);
    TEST_ASSERT_EQUAL_UINT(1U, action_slot_releases);
    TEST_ASSERT_EQUAL_UINT(reads + 1U, get_calls);
    TEST_ASSERT_EQUAL_STRING("/v1/device/notes?page=1", last_path);
    TEST_ASSERT_EQUAL(MODULES_SCREEN_LIST, latest()->screen);
    TEST_ASSERT_EQUAL(MODULES_READY, latest()->status);
    TEST_ASSERT_EQUAL(MODULES_NOTICE_DONE, latest()->notice);
    TEST_ASSERT_EQUAL(MODULES_ACTION_DELETE, latest()->action);
    TEST_ASSERT_NULL(detail_slot);
}

static void complete_is_an_empty_post_with_the_long_deadline(void)
{
    open_detail(BRIDGE_RESOURCE_TASKS, TASKS_PAGE, 5U, TASK_DETAIL);
    TEST_ASSERT_TRUE(latest()->can_complete);
    reply(200, TASKS_PAGE);
    act_reply(200, "{\"id\":5,\"status\":\"completed\"}");
    modules_complete(latest()->ticket);
    TEST_ASSERT_EQUAL_INT(BRIDGE_HTTP_POST_EMPTY, last_method);
    TEST_ASSERT_EQUAL_UINT32(BRIDGE_RESOURCE_COMPLETE_IO_MS, last_action_io_ms);
    TEST_ASSERT_EQUAL(MODULES_NOTICE_DONE, latest()->notice);
    TEST_ASSERT_EQUAL(MODULES_SCREEN_LIST, latest()->screen);
}

static void snooze_sends_the_shown_time_and_rereads_the_detail(void)
{
    open_detail(BRIDGE_RESOURCE_ALERTS, ALERTS_PAGE, 7U, ALERT_DETAIL);
    TEST_ASSERT_TRUE(latest()->can_snooze);
    act_reply(200, "{\"alert_id\":7,\"scheduled_at\":\"2026-11-02T13:00:00.000Z\"}");
    reply(200, ALERT_DETAIL);
    modules_snooze(latest()->ticket, BRIDGE_SNOOZE_ONE_HOUR);
    TEST_ASSERT_EQUAL_STRING("{\"preset\":\"snooze_1h\",\"expected_scheduled_at\":\"" T0 "\"}", last_request);
    TEST_ASSERT_EQUAL_STRING("/v1/device/alerts/7", last_path);
    TEST_ASSERT_EQUAL(MODULES_SCREEN_DETAIL, latest()->screen);
    TEST_ASSERT_EQUAL(MODULES_READY, latest()->status);
    TEST_ASSERT_EQUAL(MODULES_NOTICE_DONE, latest()->notice);
    TEST_ASSERT_EQUAL_STRING("2026-11-02T13:00:00.000Z", latest()->snoozed_until);
}

static void outcomes_tell_busy_unsent_and_uncertain_apart(void)
{
    open_detail(BRIDGE_RESOURCE_NOTES, NOTES_PAGE, 12U, NOTE_DETAIL);
    action_slot_taken = true;
    modules_delete(latest()->ticket);
    TEST_ASSERT_EQUAL_UINT(0U, actions_sent);
    TEST_ASSERT_EQUAL(MODULES_NOTICE_BUSY, latest()->notice);
    TEST_ASSERT_TRUE(latest()->can_delete);
    action_slot_taken = false;
    next_action = (bridge_http_mutation_t){.response.result = BRIDGE_HTTP_BUSY, .unsent = true};
    modules_delete(latest()->ticket);
    TEST_ASSERT_EQUAL(MODULES_NOTICE_BUSY, latest()->notice);
    next_action = (bridge_http_mutation_t){.response.result = BRIDGE_HTTP_TRANSPORT_ERROR, .unsent = true};
    modules_delete(latest()->ticket);
    TEST_ASSERT_EQUAL(MODULES_NOTICE_NOT_SENT, latest()->notice);
    TEST_ASSERT_TRUE(latest()->can_delete);
    next_action = (bridge_http_mutation_t){.response.result = BRIDGE_HTTP_TIMEOUT};
    modules_delete(latest()->ticket);
    TEST_ASSERT_EQUAL(MODULES_NOTICE_UNCONFIRMED, latest()->notice);
    TEST_ASSERT_FALSE(latest()->can_delete);
    TEST_ASSERT_EQUAL(MODULES_SCREEN_DETAIL, latest()->screen);
    TEST_ASSERT_EQUAL_UINT(3U, actions_sent);
    TEST_ASSERT_EQUAL_UINT(3U, action_slot_releases);
    /* No further action until the detail is read again. */
    modules_delete(latest()->ticket);
    TEST_ASSERT_EQUAL_UINT(3U, actions_sent);
}

int main(void)
{
    TEST_ASSERT_EQUAL(ESP_OK, modules_start(on_view));
    UNITY_BEGIN();
    RUN_TEST(entering_reads_the_first_page_and_exposes_it_to_the_reader);
    RUN_TEST(a_detail_is_read_by_id_and_back_keeps_the_list_without_a_request);
    RUN_TEST(audio_in_use_prevents_the_request_and_refresh_retries);
    RUN_TEST(transport_and_codec_failures_map_to_distinct_errors);
    RUN_TEST(a_result_that_arrives_after_leaving_is_freed_without_a_view);
    RUN_TEST(the_newest_request_runs_after_the_one_in_flight);
    RUN_TEST(a_worker_that_cannot_start_fails_the_view_and_frees_the_slot);
    RUN_TEST(paging_replaces_the_list_and_keeps_one_slot);
    RUN_TEST(a_confirmed_delete_reads_a_fresh_origin_page);
    RUN_TEST(complete_is_an_empty_post_with_the_long_deadline);
    RUN_TEST(snooze_sends_the_shown_time_and_rereads_the_detail);
    RUN_TEST(outcomes_tell_busy_unsent_and_uncertain_apart);
    return UNITY_END();
}
