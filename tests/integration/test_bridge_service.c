#include <string.h>
#include "unity.h"
#include "ui_bridge_status.h"
#include "../../components/bridge/src/bridge.c"

static bool shown_online;
static unsigned publications;
static bool drop_during_publication;
static bool change_origin_during_publication;
static ui_bridge_status_t visible_status;
unsigned bridge_test_critical_depth;
static void show_online(bool online, uint64_t generation)
{
    TEST_ASSERT_EQUAL_UINT(0, bridge_test_critical_depth);
    ++publications;
    if (online && drop_during_publication) bridge_set_ip(false);
    if (online && change_origin_during_publication) {
        bridge_origin_t origin = {"https://new.example.test", "01234567890123456789012345678901"};
        origin_changed(&origin);
    }
    ui_bridge_status_update(&visible_status, online, generation);
    shown_online = visible_status.online;
}

esp_err_t bridge_http_init(void) { return ESP_OK; }
esp_err_t bridge_clock_start(bridge_clock_synced_fn callback, void *context) { (void)callback; (void)context; return ESP_OK; }
bool bridge_clock_ready(void) { return true; }
esp_err_t bridge_origin_load(bridge_origin_t *origin) { memset(origin, 0, sizeof(*origin)); return ESP_FAIL; }
void bridge_origin_set_changed_callback(bridge_origin_changed_fn callback) { (void)callback; }
bridge_stream_result_t bridge_stream_run(const bridge_origin_t *origin, bridge_sse_t *state,
                                         const bridge_stream_hooks_t *hooks)
{ (void)origin; (void)state; (void)hooks; return (bridge_stream_result_t){0}; }
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
    drop_during_publication = false;
    change_origin_during_publication = false;
    bridge_test_critical_depth = 0;
    memset(&visible_status, 0, sizeof(visible_status));
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
    TEST_ASSERT_TRUE(stream_current(&stream));
    /* A disconnect arrives after the worker checked its ticket and while a read may be blocked. */
    bridge_set_ip(false);
    TEST_ASSERT_FALSE(shown_online);
    TEST_ASSERT_FALSE(stream_current(&stream));
    /* A callback from the old open may arrive after the disconnect callback. */
    show_online(true, stream.ticket);
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
    TEST_ASSERT_TRUE(shown_online);
    origin_changed(&origin);
    TEST_ASSERT_FALSE(shown_online);
    TEST_ASSERT_FALSE(stream_opened(&stream));
    TEST_ASSERT_FALSE(shown_online);
}

static void lost_ip_during_open_publication_is_retracted(void)
{
    bridge_origin_t origin = {"https://example.test", "01234567890123456789012345678901"};
    origin_changed(&origin);
    bridge_set_ip(true);
    stream_context_t stream = {.ticket = bridge_connection_begin_stream(&connection)};
    drop_during_publication = true;
    TEST_ASSERT_TRUE(stream_opened(&stream));
    TEST_ASSERT_FALSE(shown_online);
    TEST_ASSERT_EQUAL_UINT(4, publications);
}

static void ip_change_after_failed_start_has_no_callback(void)
{
    online_callback = NULL;
    bridge_set_ip(true);
    TEST_ASSERT_EQUAL_UINT(0, publications);
}

static void origin_change_during_open_publication_wins(void)
{
    bridge_origin_t origin = {"https://example.test", "01234567890123456789012345678901"};
    origin_changed(&origin);
    bridge_set_ip(true);
    stream_context_t stream = {.ticket = bridge_connection_begin_stream(&connection)};
    change_origin_during_publication = true;
    TEST_ASSERT_TRUE(stream_opened(&stream));
    TEST_ASSERT_FALSE(shown_online);
    TEST_ASSERT_FALSE(stream_current(&stream));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(lost_ip_invalidates_a_stale_open);
    RUN_TEST(origin_change_invalidates_a_stale_open);
    RUN_TEST(lost_ip_during_open_publication_is_retracted);
    RUN_TEST(ip_change_after_failed_start_has_no_callback);
    RUN_TEST(origin_change_during_open_publication_wins);
    return UNITY_END();
}
