#include <string.h>
#include "provision_line.h"
#include "unity.h"

static char buffer[PROVISION_LINE_MAX_BYTES + 1];
static provision_line_t line;

void setUp(void)
{
    provision_line_init(&line, buffer);
}

void tearDown(void) {}

static provision_line_status_t feed_text(const char *text, uint64_t now_ms)
{
    provision_line_status_t status = PROVISION_LINE_PENDING;
    for (const char *c = text; *c != '\0'; ++c) {
        status = provision_line_feed(&line, *c, now_ms);
    }
    return status;
}

/* The request points into the parsed line, so the copy outlives the call. */
static char parsed_line[PROVISION_LINE_MAX_BYTES + 1];

static bool parse(const char *text, provision_request_t *request)
{
    strcpy(parsed_line, text);
    return provision_request_parse(parsed_line, request);
}

static void test_lf_ends_a_line_and_cr_is_ignored(void)
{
    TEST_ASSERT_EQUAL(PROVISION_LINE_PENDING, feed_text("ab\rc", 0));
    TEST_ASSERT_EQUAL(PROVISION_LINE_READY, provision_line_feed(&line, '\n', 0));
    TEST_ASSERT_EQUAL_STRING("abc", buffer);
    provision_line_clear(&line);
    TEST_ASSERT_EQUAL_size_t(0, line.length);
    TEST_ASSERT_EACH_EQUAL_CHAR('\0', buffer, sizeof(buffer));
}

static void test_a_line_of_3072_bytes_fits_and_a_longer_one_is_rejected_whole(void)
{
    for (unsigned i = 0; i < PROVISION_LINE_MAX_BYTES; ++i) {
        provision_line_feed(&line, 'x', 0);
    }
    TEST_ASSERT_EQUAL(PROVISION_LINE_READY, provision_line_feed(&line, '\n', 0));
    TEST_ASSERT_EQUAL_size_t(PROVISION_LINE_MAX_BYTES, strlen(buffer));
    provision_line_clear(&line);

    for (unsigned i = 0; i <= PROVISION_LINE_MAX_BYTES + 10U; ++i) {
        provision_line_feed(&line, 'x', 0);
    }
    TEST_ASSERT_EACH_EQUAL_CHAR('\0', buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(PROVISION_LINE_REJECTED, provision_line_feed(&line, '\n', 0));
    /* The next line starts clean. */
    TEST_ASSERT_EQUAL(PROVISION_LINE_READY, feed_text("ok\n", 0));
    TEST_ASSERT_EQUAL_STRING("ok", buffer);
}

static void test_a_nul_byte_rejects_the_whole_line(void)
{
    feed_text("{\"v\":1}", 0);
    provision_line_feed(&line, '\0', 0);
    feed_text("junk", 0);
    TEST_ASSERT_EQUAL(PROVISION_LINE_REJECTED, provision_line_feed(&line, '\n', 0));
    TEST_ASSERT_EACH_EQUAL_CHAR('\0', buffer, sizeof(buffer));
}

static void test_a_partial_line_idle_for_10_seconds_is_dropped(void)
{
    feed_text("secret", 1000);
    provision_line_expire(&line, 1000 + PROVISION_LINE_IDLE_MS - 1);
    TEST_ASSERT_EQUAL_size_t(6, line.length);
    provision_line_expire(&line, 1000 + PROVISION_LINE_IDLE_MS);
    TEST_ASSERT_EQUAL_size_t(0, line.length);
    TEST_ASSERT_EACH_EQUAL_CHAR('\0', buffer, sizeof(buffer));

    /* A late byte after the idle time also starts a new line. */
    feed_text("old", 0);
    TEST_ASSERT_EQUAL(PROVISION_LINE_READY, feed_text("new\n", PROVISION_LINE_IDLE_MS));
    TEST_ASSERT_EQUAL_STRING("new", buffer);
}

static void test_configure_bridge_needs_exactly_its_fields(void)
{
    provision_request_t request;
    TEST_ASSERT_TRUE(parse(" {\"v\":1,\"op\":\"configure_bridge\",\"bridge_url\":\"https://h\",\"device_token\":\"t\"} ",
                           &request));
    TEST_ASSERT_EQUAL(PROVISION_OP_CONFIGURE_BRIDGE, request.op);
    TEST_ASSERT_EQUAL_STRING("https://h", request.bridge_url);
    TEST_ASSERT_EQUAL_STRING("t", request.device_token);
    TEST_ASSERT_NULL(request.wifi_ssid);

    TEST_ASSERT_FALSE(parse("{\"v\":1,\"op\":\"configure_bridge\",\"bridge_url\":\"u\"}", &request));
    TEST_ASSERT_FALSE(parse("{\"v\":1,\"op\":\"configure_bridge\",\"bridge_url\":\"u\",\"device_token\":\"t\","
                            "\"wifi_ssid\":\"s\"}",
                            &request));
    TEST_ASSERT_FALSE(parse("{\"v\":1,\"op\":\"configure_bridge\",\"bridge_url\":\"u\",\"device_token\":\"t\","
                            "\"extra\":\"x\"}",
                            &request));
}

static void test_configure_needs_exactly_its_fields(void)
{
    provision_request_t request;
    const char *full = "{\"op\":\"configure\",\"v\":1,\"wifi_ssid\":\"Home\",\"wifi_password\":\"\","
                       "\"bridge_url\":\"https://h/p\",\"device_token\":\"t\",\"screen_timeout_s\":60}";
    TEST_ASSERT_TRUE(parse(full, &request));
    TEST_ASSERT_EQUAL(PROVISION_OP_CONFIGURE, request.op);
    TEST_ASSERT_EQUAL_STRING("Home", request.wifi_ssid);
    TEST_ASSERT_EQUAL_STRING("", request.wifi_password);
    TEST_ASSERT_EQUAL_UINT32(60, request.screen_timeout_s);

    TEST_ASSERT_FALSE(parse("{\"v\":1,\"op\":\"configure\",\"wifi_ssid\":\"Home\",\"wifi_password\":\"\","
                            "\"bridge_url\":\"u\",\"device_token\":\"t\"}",
                            &request));
}

static void test_version_op_and_types_are_strict(void)
{
    provision_request_t request;
    const char *cases[] = {
        "{\"v\":2,\"op\":\"configure_bridge\",\"bridge_url\":\"u\",\"device_token\":\"t\"}",
        "{\"v\":\"1\",\"op\":\"configure_bridge\",\"bridge_url\":\"u\",\"device_token\":\"t\"}",
        "{\"v\":01,\"op\":\"configure_bridge\",\"bridge_url\":\"u\",\"device_token\":\"t\"}",
        "{\"v\":1.0,\"op\":\"configure_bridge\",\"bridge_url\":\"u\",\"device_token\":\"t\"}",
        "{\"v\":1,\"op\":\"reset\",\"bridge_url\":\"u\",\"device_token\":\"t\"}",
        "{\"v\":1,\"op\":\"configure_bridge\",\"bridge_url\":1,\"device_token\":\"t\"}",
        "{\"v\":1,\"v\":1,\"op\":\"configure_bridge\",\"bridge_url\":\"u\",\"device_token\":\"t\"}",
        "{\"v\":1,\"op\":\"configure_bridge\",\"bridge_url\":\"u\",\"device_token\":\"t\"} x",
        "{\"v\":1,\"op\":\"configure_bridge\",\"bridge_url\":\"u\",\"device_token\":\"t\",}",
        "{\"v\":1,\"op\":\"configure_bridge\",\"bridge_url\":\"u\",\"device_token\":\"t\"",
        "[{\"v\":1}]",
        "{\"v\":1,\"op\":\"configure_bridge\",\"bridge_url\":\"u\",\"device_token\":{\"a\":\"b\"}}",
        "{\"v\":99999999999,\"op\":\"configure_bridge\",\"bridge_url\":\"u\",\"device_token\":\"t\"}",
        "",
    };
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        TEST_ASSERT_FALSE_MESSAGE(parse(cases[i], &request), cases[i]);
    }
}

static void test_string_escapes_decode_in_place(void)
{
    provision_request_t request;
    TEST_ASSERT_TRUE(parse("{\"v\":1,\"op\":\"configure_bridge\",\"bridge_url\":\"a\\\"b\\\\c\\/d\\n\","
                           "\"device_token\":\"caf\\u00e9 \\u20ac \\ud83d\\ude00\"}",
                           &request));
    TEST_ASSERT_EQUAL_STRING("a\"b\\c/d\n", request.bridge_url);
    TEST_ASSERT_EQUAL_STRING("caf\xc3\xa9 \xe2\x82\xac \xf0\x9f\x98\x80", request.device_token);
}

static void test_invalid_escapes_raw_controls_and_nul_are_rejected(void)
{
    provision_request_t request;
    const char *cases[] = {
        "{\"v\":1,\"op\":\"configure_bridge\",\"bridge_url\":\"\\u0000\",\"device_token\":\"t\"}",
        "{\"v\":1,\"op\":\"configure_bridge\",\"bridge_url\":\"\\x41\",\"device_token\":\"t\"}",
        "{\"v\":1,\"op\":\"configure_bridge\",\"bridge_url\":\"\\u12\",\"device_token\":\"t\"}",
        "{\"v\":1,\"op\":\"configure_bridge\",\"bridge_url\":\"\\ud83d\",\"device_token\":\"t\"}",
        "{\"v\":1,\"op\":\"configure_bridge\",\"bridge_url\":\"\\ude00\",\"device_token\":\"t\"}",
        "{\"v\":1,\"op\":\"configure_bridge\",\"bridge_url\":\"\\ud83d\\u0041\",\"device_token\":\"t\"}",
        "{\"v\":1,\"op\":\"configure_bridge\",\"bridge_url\":\"tab\there\",\"device_token\":\"t\"}",
    };
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        TEST_ASSERT_FALSE_MESSAGE(parse(cases[i], &request), cases[i]);
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_lf_ends_a_line_and_cr_is_ignored);
    RUN_TEST(test_a_line_of_3072_bytes_fits_and_a_longer_one_is_rejected_whole);
    RUN_TEST(test_a_nul_byte_rejects_the_whole_line);
    RUN_TEST(test_a_partial_line_idle_for_10_seconds_is_dropped);
    RUN_TEST(test_configure_bridge_needs_exactly_its_fields);
    RUN_TEST(test_configure_needs_exactly_its_fields);
    RUN_TEST(test_version_op_and_types_are_strict);
    RUN_TEST(test_string_escapes_decode_in_place);
    RUN_TEST(test_invalid_escapes_raw_controls_and_nul_are_rejected);
    return UNITY_END();
}
