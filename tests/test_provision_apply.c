#include <string.h>
#include "provision_apply.h"
#include "unity.h"

static char calls[64];
static bool origin_result;
static bool timeout_result;
static bool network_result;

static void record(char call)
{
    const size_t length = strlen(calls);
    calls[length] = call;
    calls[length + 1] = '\0';
}

static bool timeout_valid(uint32_t seconds)
{
    /* Accepts the value that would wrap to 15, so the range check belongs to provision_apply. */
    return seconds == 0 || seconds == 15 || seconds == 30 || seconds == 60 || seconds == 256 + 15;
}

static bool save_origin(const char *url, const char *token)
{
    (void)url;
    (void)token;
    record('o');
    return origin_result;
}

static bool store_timeout(uint8_t seconds)
{
    (void)seconds;
    record('t');
    return timeout_result;
}

static bool save_network(const char *ssid, const char *password)
{
    (void)ssid;
    (void)password;
    record('n');
    return network_result;
}

static const provision_actions_t ACTIONS = {
    .timeout_valid = timeout_valid,
    .save_origin = save_origin,
    .store_timeout = store_timeout,
    .save_network = save_network,
};

static const char TOKEN[] = "0123456789abcdef0123456789abcdef";

static provision_request_t configure_request(void)
{
    return (provision_request_t){
        .op = PROVISION_OP_CONFIGURE,
        .wifi_ssid = "Home",
        .wifi_password = "password",
        .bridge_url = "https://bridge.example.com/v1",
        .device_token = TOKEN,
        .screen_timeout_s = 60,
    };
}

void setUp(void)
{
    calls[0] = '\0';
    origin_result = timeout_result = network_result = true;
}

void tearDown(void) {}

static void test_configure_writes_origin_then_timeout_then_network(void)
{
    const provision_request_t request = configure_request();
    TEST_ASSERT_TRUE(provision_apply(&request, &ACTIONS));
    TEST_ASSERT_EQUAL_STRING("otn", calls);
}

static void test_configure_bridge_writes_only_the_origin(void)
{
    const provision_request_t request = {
        .op = PROVISION_OP_CONFIGURE_BRIDGE, .bridge_url = "https://h", .device_token = TOKEN};
    TEST_ASSERT_TRUE(provision_apply(&request, &ACTIONS));
    TEST_ASSERT_EQUAL_STRING("o", calls);
}

static void test_any_invalid_value_writes_nothing(void)
{
    provision_request_t request = configure_request();
    request.screen_timeout_s = 45;
    TEST_ASSERT_FALSE(provision_apply(&request, &ACTIONS));
    request = configure_request();
    request.wifi_ssid = "";
    TEST_ASSERT_FALSE(provision_apply(&request, &ACTIONS));
    request = configure_request();
    request.wifi_password = "short";
    TEST_ASSERT_FALSE(provision_apply(&request, &ACTIONS));
    request = configure_request();
    request.bridge_url = "http://h";
    TEST_ASSERT_FALSE(provision_apply(&request, &ACTIONS));
    request = configure_request();
    request.device_token = "short";
    TEST_ASSERT_FALSE(provision_apply(&request, &ACTIONS));
    request = configure_request();
    request.screen_timeout_s = 256 + 15;
    TEST_ASSERT_FALSE(provision_apply(&request, &ACTIONS));
    TEST_ASSERT_EQUAL_STRING("", calls);
}

static void test_the_first_failed_write_stops_and_keeps_what_was_written(void)
{
    const provision_request_t request = configure_request();
    timeout_result = false;
    TEST_ASSERT_FALSE(provision_apply(&request, &ACTIONS));
    TEST_ASSERT_EQUAL_STRING("ot", calls);

    calls[0] = '\0';
    timeout_result = true;
    network_result = false;
    TEST_ASSERT_FALSE(provision_apply(&request, &ACTIONS));
    TEST_ASSERT_EQUAL_STRING("otn", calls);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_configure_writes_origin_then_timeout_then_network);
    RUN_TEST(test_configure_bridge_writes_only_the_origin);
    RUN_TEST(test_any_invalid_value_writes_nothing);
    RUN_TEST(test_the_first_failed_write_stops_and_keeps_what_was_written);
    return UNITY_END();
}
