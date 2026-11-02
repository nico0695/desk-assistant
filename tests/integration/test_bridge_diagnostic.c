#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "unity.h"
#include "../../components/bridge/src/bridge_diagnostic.c"

static void (*scheduled)(void *);
static void *scheduled_argument;
static unsigned reads, checkpoints, deletions;
static bool current;
static char captured_log[512];

BaseType_t xTaskCreate(void (*task)(void *), const char *name, unsigned stack, void *argument,
                       unsigned priority, TaskHandle_t *handle)
{
    (void)name; (void)stack; (void)priority; (void)handle;
    scheduled = task;
    scheduled_argument = argument;
    return pdPASS;
}
void vTaskDelete(TaskHandle_t task) { (void)task; ++deletions; }
void memory_report_log(const char *checkpoint) { (void)checkpoint; ++checkpoints; }
void test_log(const char *tag, const char *format, ...)
{
    (void)tag;
    va_list args;
    va_start(args, format);
    vsnprintf(captured_log, sizeof(captured_log), format, args);
    va_end(args);
}
static bool connection_current(uint64_t ticket) { return current && ticket == 7U; }
bridge_http_response_t bridge_http_get(const bridge_origin_t *origin, const char *path,
                                       uint32_t gate, uint32_t io, char *body, size_t capacity)
{
    ++reads;
    TEST_ASSERT_EQUAL_STRING("https://bridge.example", origin->url);
    TEST_ASSERT_EQUAL_STRING("/v1/device/events?limit=1", path);
    TEST_ASSERT_EQUAL_UINT(BRIDGE_HTTP_EVENT_GATE_MS, gate);
    TEST_ASSERT_EQUAL_UINT(BRIDGE_HTTP_EVENT_IO_MS, io);
    TEST_ASSERT_EQUAL_UINT(BRIDGE_HTTP_EVENT_BODY_MAX_BYTES, capacity);
    memcpy(body, "private content", sizeof("private content"));
    return (bridge_http_response_t){.result = BRIDGE_HTTP_OK, .status = 200, .length = 15};
}
void setUp(void)
{
    attempted = false;
    scheduled = NULL;
    scheduled_argument = NULL;
    reads = checkpoints = deletions = 0;
    current = true;
    captured_log[0] = '\0';
}
void tearDown(void) {}

static void runs_once_with_copied_origin_and_no_content_logging(void)
{
    bridge_origin_t origin = {.url = "https://bridge.example", .token = "private token"};
    bridge_diagnostic_start(&origin, 7U, connection_current);
    TEST_ASSERT_NOT_NULL(scheduled);
    memset(&origin, 0, sizeof(origin));
    scheduled(scheduled_argument);
    TEST_ASSERT_EQUAL_UINT(1, reads);
    TEST_ASSERT_EQUAL_UINT(3, checkpoints);
    TEST_ASSERT_EQUAL_UINT(1, deletions);
    TEST_ASSERT_NULL(strstr(captured_log, "private"));
    bridge_diagnostic_start(&origin, 7U, connection_current);
    TEST_ASSERT_EQUAL_UINT(1, reads);
}

static void stale_connection_prevents_the_read(void)
{
    bridge_origin_t origin = {.url = "https://bridge.example"};
    bridge_diagnostic_start(&origin, 7U, connection_current);
    current = false;
    scheduled(scheduled_argument);
    TEST_ASSERT_EQUAL_UINT(0, reads);
    TEST_ASSERT_EQUAL_UINT(1, deletions);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(runs_once_with_copied_origin_and_no_content_logging);
    RUN_TEST(stale_connection_prevents_the_read);
    return UNITY_END();
}
