#include <limits.h>

#include "bridge_http_common.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static void url_preserves_prefix_and_rejects_untrusted_paths(void)
{
    char url[BRIDGE_HTTP_URL_SIZE];
    TEST_ASSERT_TRUE(bridge_http_make_url("https://example.test/prefix/", "/v1/device/events?limit=1", url, sizeof(url)));
    TEST_ASSERT_EQUAL_STRING("https://example.test/prefix/v1/device/events?limit=1", url);
    TEST_ASSERT_TRUE(bridge_http_make_url("https://example.test", "/v1/device/event-stream", url, sizeof(url)));
    TEST_ASSERT_EQUAL_STRING("https://example.test/v1/device/event-stream", url);
    TEST_ASSERT_FALSE(bridge_http_make_url("http://example.test", "/v1/device/events", url, sizeof(url)));
    TEST_ASSERT_FALSE(bridge_http_make_url("https://example.test", "https://other.test", url, sizeof(url)));
    TEST_ASSERT_FALSE(bridge_http_make_url("https://example.test", "/v1/device/events\r\nHeader:x", url, sizeof(url)));
    TEST_ASSERT_FALSE(bridge_http_make_url("https://example.test", "/v1/device/events#fragment", url, sizeof(url)));
    TEST_ASSERT_FALSE(bridge_http_make_url("https://example.test/prefix", "/v1/device/../other", url, sizeof(url)));
    TEST_ASSERT_FALSE(bridge_http_make_url("https://example.test/prefix", "/v1/device/%2e%2e/other", url, sizeof(url)));
    TEST_ASSERT_FALSE(bridge_http_make_url("https://example.test", "/v1/device/events", url, 8));
}

static void remaining_time_never_extends_deadline(void)
{
    TEST_ASSERT_EQUAL_INT(0, bridge_http_remaining_ms(1000000, 1000000, 5000));
    TEST_ASSERT_EQUAL_INT(0, bridge_http_remaining_ms(1000001, 1000000, 5000));
    TEST_ASSERT_EQUAL_INT(999, bridge_http_remaining_ms(2000000, 1000001, 5000));
    TEST_ASSERT_EQUAL_INT(50, bridge_http_remaining_ms(2000000, 1000000, 50));
    TEST_ASSERT_EQUAL_INT(INT_MAX, bridge_http_remaining_ms(INT64_C(3000000000000), 0, -1));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(url_preserves_prefix_and_rejects_untrusted_paths);
    RUN_TEST(remaining_time_never_extends_deadline);
    return UNITY_END();
}
