#include <string.h>
#include "unity.h"
#include "../../components/bridge/src/bridge.c"

static bool shown_online;
static unsigned publications;
static void show_online(bool online) { shown_online = online; ++publications; }

esp_err_t bridge_http_init(void) { return ESP_OK; }
esp_err_t bridge_clock_start(bridge_clock_synced_fn callback, void *context) { (void)callback; (void)context; return ESP_OK; }
bool bridge_clock_ready(void) { return true; }
esp_err_t bridge_origin_load(bridge_origin_t *origin) { memset(origin, 0, sizeof(*origin)); return ESP_FAIL; }
void bridge_origin_set_changed_callback(bridge_origin_changed_fn callback) { (void)callback; }
void bridge_stream_run(const bridge_origin_t *origin, bridge_sse_t *state, const bridge_stream_hooks_t *hooks)
{ (void)origin; (void)state; (void)hooks; }
int64_t esp_timer_get_time(void) { return 0; }
BaseType_t xTaskCreate(void (*task)(void *), const char *name, unsigned stack, void *arg, unsigned priority, TaskHandle_t *handle)
{ (void)task; (void)name; (void)stack; (void)arg; (void)priority; *handle = (void *)1; return pdPASS; }
void xTaskNotifyGive(TaskHandle_t task) { (void)task; }
unsigned ulTaskNotifyTake(BaseType_t clear, TickType_t timeout) { (void)clear; (void)timeout; return 0; }

void setUp(void)
{
    bridge_connection_init(&connection);
    memset(&active_origin, 0, sizeof(active_origin));
    input_revision = 0;
    stream_task = NULL;
    online_callback = show_online;
    shown_online = false;
    publications = 0;
}
void tearDown(void) {}

static void lost_ip_invalidates_a_stale_open(void)
{
    bridge_origin_t origin = {"https://example.test", "01234567890123456789012345678901"};
    origin_changed(&origin);
    bridge_set_ip(true);
    stream_context_t stream = {.ticket = bridge_connection_begin_stream(&connection)};
    TEST_ASSERT_TRUE(stream_opened(&stream));
    TEST_ASSERT_TRUE(shown_online);
    bridge_set_ip(false);
    TEST_ASSERT_FALSE(shown_online);
    TEST_ASSERT_FALSE(stream_opened(&stream));
    TEST_ASSERT_FALSE(shown_online);
}

static void origin_change_invalidates_a_stale_open(void)
{
    bridge_origin_t origin = {"https://example.test", "01234567890123456789012345678901"};
    origin_changed(&origin);
    bridge_set_ip(true);
    stream_context_t stream = {.ticket = bridge_connection_begin_stream(&connection)};
    TEST_ASSERT_TRUE(stream_opened(&stream));
    origin_changed(&origin);
    TEST_ASSERT_FALSE(shown_online);
    TEST_ASSERT_FALSE(stream_opened(&stream));
    TEST_ASSERT_FALSE(shown_online);
}

static void ip_change_after_failed_start_has_no_callback(void)
{
    online_callback = NULL;
    bridge_set_ip(true);
    TEST_ASSERT_EQUAL_UINT(0, publications);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(lost_ip_invalidates_a_stale_open);
    RUN_TEST(origin_change_invalidates_a_stale_open);
    RUN_TEST(ip_change_after_failed_start_has_no_callback);
    return UNITY_END();
}
