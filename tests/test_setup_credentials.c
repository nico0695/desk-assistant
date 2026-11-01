#include <string.h>
#include "setup_credentials.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static uint32_t sequence;

static uint32_t counting_random(void)
{
    return sequence++;
}

static uint32_t constant_random(void)
{
    return 0xFFFFFFFFU;
}

static void test_ap_ssid_ends_with_the_last_two_mac_bytes(void)
{
    const uint8_t mac[6] = {0xd4, 0x05, 0x92, 0xb6, 0xed, 0x28};
    char ssid[SETUP_AP_SSID_SIZE];
    setup_ap_ssid(mac, ssid);
    TEST_ASSERT_EQUAL_STRING("Desk-Assistant-ED28", ssid);
}

static void test_ap_password_uses_only_unambiguous_symbols(void)
{
    char password[SETUP_AP_PASSWORD_SIZE];
    sequence = 0;
    for (unsigned round = 0; round < 8; ++round) {
        setup_ap_password(counting_random, password);
        TEST_ASSERT_EQUAL_size_t(SETUP_AP_PASSWORD_LENGTH, strlen(password));
        for (unsigned i = 0; i < SETUP_AP_PASSWORD_LENGTH; ++i) {
            TEST_ASSERT_NOT_NULL(strchr("abcdefghjkmnpqrstuvwxyz23456789", password[i]));
            TEST_ASSERT_NULL(strchr("01oli", password[i]));
        }
    }
    /* The largest random value still maps inside the alphabet. */
    setup_ap_password(constant_random, password);
    TEST_ASSERT_EQUAL_STRING("dddddddd", password);
}

static void test_nonce_is_lowercase_hex(void)
{
    char nonce[SETUP_NONCE_SIZE];
    sequence = 0;
    setup_nonce(counting_random, nonce);
    TEST_ASSERT_EQUAL_STRING("0123456789abcdef0123456789abcdef", nonce);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_ap_ssid_ends_with_the_last_two_mac_bytes);
    RUN_TEST(test_ap_password_uses_only_unambiguous_symbols);
    RUN_TEST(test_nonce_is_lowercase_hex);
    return UNITY_END();
}
