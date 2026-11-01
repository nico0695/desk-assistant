#include <string.h>
#include "setup_form.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static const char NONCE[] = "0123456789abcdef0123456789abcdef";
static char body[SETUP_FORM_MAX_BYTES + 1];

static setup_form_result_t parse(const char *text, setup_form_t *form)
{
    strcpy(body, text);
    return setup_form_parse(body, strlen(body), NONCE, form);
}

static void test_fields_decode_in_any_order(void)
{
    setup_form_t form;
    TEST_ASSERT_EQUAL(SETUP_FORM_OK,
                      parse("ssid=My+Home%21&password=p%26ss%3Dw+rd&nonce=0123456789abcdef0123456789abcdef", &form));
    TEST_ASSERT_EQUAL_STRING("My Home!", form.ssid);
    TEST_ASSERT_EQUAL_STRING("p&ss=w rd", form.password);
}

static void test_an_empty_password_is_kept_for_open_networks(void)
{
    setup_form_t form;
    TEST_ASSERT_EQUAL(SETUP_FORM_OK, parse("nonce=0123456789abcdef0123456789abcdef&ssid=Cafe&password=", &form));
    TEST_ASSERT_EQUAL_STRING("", form.password);
}

static void test_a_wrong_or_missing_session_nonce_is_stale(void)
{
    setup_form_t form;
    TEST_ASSERT_EQUAL(SETUP_FORM_STALE_NONCE, parse("nonce=ffff&ssid=a&password=", &form));
    TEST_ASSERT_NULL(form.ssid);
    strcpy(body, "nonce=&ssid=a&password=");
    TEST_ASSERT_EQUAL(SETUP_FORM_STALE_NONCE, setup_form_parse(body, strlen(body), "", &form));
}

static void test_malformed_bodies_are_rejected(void)
{
    setup_form_t form;
    const char *cases[] = {
        "nonce=0123456789abcdef0123456789abcdef&ssid=a",
        "nonce=0123456789abcdef0123456789abcdef&ssid=a&password=&ssid=b",
        "nonce=0123456789abcdef0123456789abcdef&ssid=a&password=&extra=1",
        "nonce=0123456789abcdef0123456789abcdef&ssid=a&password",
        "nonce=0123456789abcdef0123456789abcdef&ssid=a%00b&password=",
        "nonce=0123456789abcdef0123456789abcdef&ssid=a%2&password=",
        "nonce=0123456789abcdef0123456789abcdef&ssid=a%zz&password=",
        "nonce=0123456789abcdef0123456789abcdef&ssid=a&password=&",
        "",
    };
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        TEST_ASSERT_EQUAL_MESSAGE(SETUP_FORM_MALFORMED, parse(cases[i], &form), cases[i]);
    }
}

static void test_a_full_1024_byte_body_is_parsed_and_a_longer_one_rejected(void)
{
    setup_form_t form;
    static const char prefix[] = "nonce=0123456789abcdef0123456789abcdef&password=&ssid=";
    const size_t prefix_length = sizeof(prefix) - 1U;
    memcpy(body, prefix, prefix_length);
    memset(body + prefix_length, 's', SETUP_FORM_MAX_BYTES - prefix_length);
    TEST_ASSERT_EQUAL(SETUP_FORM_OK, setup_form_parse(body, SETUP_FORM_MAX_BYTES, NONCE, &form));
    TEST_ASSERT_EQUAL_size_t(SETUP_FORM_MAX_BYTES - prefix_length, strlen(form.ssid));
    TEST_ASSERT_EQUAL(SETUP_FORM_MALFORMED, setup_form_parse(body, SETUP_FORM_MAX_BYTES + 1, NONCE, &form));
}

static void test_peer_must_reach_the_ap_address_from_its_subnet(void)
{
    const uint32_t ap = 0x0104A8C0U;      /* 192.168.4.1 in network byte order on a little-endian host */
    const uint32_t mask = 0x00FFFFFFU;    /* 255.255.255.0 */
    const uint32_t phone = 0x0204A8C0U;   /* 192.168.4.2 */
    const uint32_t outsider = 0x0A01A8C0U; /* 192.168.1.10 */
    TEST_ASSERT_TRUE(setup_peer_allowed(ap, phone, ap, mask));
    TEST_ASSERT_FALSE(setup_peer_allowed(ap, outsider, ap, mask));
    TEST_ASSERT_FALSE(setup_peer_allowed(outsider, phone, ap, mask));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_fields_decode_in_any_order);
    RUN_TEST(test_an_empty_password_is_kept_for_open_networks);
    RUN_TEST(test_a_wrong_or_missing_session_nonce_is_stale);
    RUN_TEST(test_malformed_bodies_are_rejected);
    RUN_TEST(test_a_full_1024_byte_body_is_parsed_and_a_longer_one_rejected);
    RUN_TEST(test_peer_must_reach_the_ap_address_from_its_subnet);
    return UNITY_END();
}
