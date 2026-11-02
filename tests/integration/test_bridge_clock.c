#include "bridge_clock.h"
#include "esp_netif_sntp.h"
#include "unity.h"

static esp_err_t init_result = ESP_FAIL;
static unsigned init_count;
static unsigned notified;
static void (*sync_callback)(struct timeval *);

esp_err_t esp_netif_sntp_init(const esp_sntp_config_t *config)
{
    ++init_count;
    sync_callback = config->sync_cb;
    return init_result;
}

static void on_synced(void *context)
{
    TEST_ASSERT_EQUAL_PTR(&notified, context);
    ++notified;
}

void setUp(void) {}
void tearDown(void) {}

static void sync_is_not_confused_with_start_and_failed_start_can_retry(void)
{
    TEST_ASSERT_FALSE(bridge_clock_ready());
    TEST_ASSERT_EQUAL_INT(ESP_FAIL, bridge_clock_start(on_synced, &notified));
    TEST_ASSERT_FALSE(bridge_clock_ready());
    init_result = ESP_OK;
    TEST_ASSERT_EQUAL_INT(ESP_OK, bridge_clock_start(on_synced, &notified));
    TEST_ASSERT_FALSE(bridge_clock_ready());
    TEST_ASSERT_NOT_NULL(sync_callback);
    sync_callback(NULL);
    TEST_ASSERT_TRUE(bridge_clock_ready());
    TEST_ASSERT_EQUAL_UINT(1, notified);
    TEST_ASSERT_EQUAL_INT(ESP_OK, bridge_clock_start(on_synced, &notified));
    TEST_ASSERT_EQUAL_UINT(2, init_count);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(sync_is_not_confused_with_start_and_failed_start_can_retry);
    return UNITY_END();
}
