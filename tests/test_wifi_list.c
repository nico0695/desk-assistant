#include <string.h>
#include "unity.h"
#include "wifi_list.h"

void setUp(void) {}
void tearDown(void) {}

static const char HEX_KEY[] = "0123456789abcdefABCDEF0123456789abcdefABCDEF0123456789abcdef0123";

static void test_ssid_is_one_to_32_bytes(void)
{
    char longest[WIFI_SSID_SIZE];
    memset(longest, 'a', WIFI_SSID_MAX_BYTES);
    longest[WIFI_SSID_MAX_BYTES] = '\0';
    TEST_ASSERT_TRUE(wifi_ssid_valid("a"));
    TEST_ASSERT_TRUE(wifi_ssid_valid(longest));
    TEST_ASSERT_FALSE(wifi_ssid_valid(""));
    TEST_ASSERT_FALSE(wifi_ssid_valid(NULL));
    char too_long[WIFI_SSID_SIZE + 1];
    memset(too_long, 'a', sizeof(too_long) - 1);
    too_long[sizeof(too_long) - 1] = '\0';
    TEST_ASSERT_FALSE(wifi_ssid_valid(too_long));
}

static void test_password_is_open_text_or_hex_key(void)
{
    TEST_ASSERT_TRUE(wifi_password_valid(""));
    TEST_ASSERT_TRUE(wifi_password_valid("12345678"));
    TEST_ASSERT_TRUE(wifi_password_valid("with space ~!"));
    TEST_ASSERT_TRUE(wifi_password_valid(HEX_KEY));
    TEST_ASSERT_FALSE(wifi_password_valid("1234567"));
    TEST_ASSERT_FALSE(wifi_password_valid("tab\tinside"));
    TEST_ASSERT_FALSE(wifi_password_valid("caf\xc3\xa9-password"));
    char text63[64];
    memset(text63, 'x', 63);
    text63[63] = '\0';
    TEST_ASSERT_TRUE(wifi_password_valid(text63));
    char text64[65];
    memset(text64, 'x', 64);
    text64[64] = '\0';
    TEST_ASSERT_FALSE(wifi_password_valid(text64));
}

static void test_save_puts_new_and_updated_networks_first(void)
{
    wifi_list_t list = {0};
    TEST_ASSERT_EQUAL(WIFI_LIST_SAVE_ADDED, wifi_list_save(&list, "home", "password1", NULL));
    TEST_ASSERT_EQUAL(WIFI_LIST_SAVE_ADDED, wifi_list_save(&list, "office", "", NULL));
    TEST_ASSERT_EQUAL_UINT8(2, list.count);
    TEST_ASSERT_EQUAL_STRING("office", list.networks[0].ssid);
    TEST_ASSERT_EQUAL(WIFI_LIST_SAVE_UPDATED, wifi_list_save(&list, "home", "password2", NULL));
    TEST_ASSERT_EQUAL_UINT8(2, list.count);
    TEST_ASSERT_EQUAL_STRING("home", list.networks[0].ssid);
    TEST_ASSERT_EQUAL_STRING("password2", list.networks[0].password);
    TEST_ASSERT_EQUAL_STRING("office", list.networks[1].ssid);
    TEST_ASSERT_TRUE(wifi_list_valid(&list));
}

static void test_a_sixth_network_replaces_the_oldest(void)
{
    wifi_list_t list = {0};
    const char *names[] = {"n1", "n2", "n3", "n4", "n5"};
    for (unsigned i = 0; i < WIFI_LIST_CAPACITY; ++i) {
        TEST_ASSERT_EQUAL(WIFI_LIST_SAVE_ADDED, wifi_list_save(&list, names[i], "password", NULL));
    }
    char replaced[WIFI_SSID_SIZE] = "";
    TEST_ASSERT_EQUAL(WIFI_LIST_SAVE_REPLACED_OLDEST, wifi_list_save(&list, "n6", "password", replaced));
    TEST_ASSERT_EQUAL_STRING("n1", replaced);
    TEST_ASSERT_EQUAL_UINT8(WIFI_LIST_CAPACITY, list.count);
    TEST_ASSERT_EQUAL_STRING("n6", list.networks[0].ssid);
    TEST_ASSERT_EQUAL_STRING("n2", list.networks[4].ssid);
    TEST_ASSERT_EQUAL(-1, wifi_list_find(&list, "n1"));
    /* Updating a saved network on a full list replaces nothing. */
    TEST_ASSERT_EQUAL(WIFI_LIST_SAVE_UPDATED, wifi_list_save(&list, "n3", "password", replaced));
    TEST_ASSERT_TRUE(wifi_list_valid(&list));
}

static void test_an_invalid_network_leaves_the_list_unchanged(void)
{
    wifi_list_t list = {0};
    wifi_list_save(&list, "home", "password1", NULL);
    const wifi_list_t before = list;
    TEST_ASSERT_EQUAL(WIFI_LIST_SAVE_INVALID, wifi_list_save(&list, "", "password1", NULL));
    TEST_ASSERT_EQUAL(WIFI_LIST_SAVE_INVALID, wifi_list_save(&list, "other", "short", NULL));
    TEST_ASSERT_EQUAL_MEMORY(&before, &list, sizeof(list));
}

static void test_forget_removes_by_ssid_and_zeroes_the_slot(void)
{
    wifi_list_t list = {0};
    wifi_list_save(&list, "a", "password", NULL);
    wifi_list_save(&list, "b", "password", NULL);
    wifi_list_save(&list, "c", "password", NULL);
    TEST_ASSERT_TRUE(wifi_list_forget(&list, "b"));
    TEST_ASSERT_FALSE(wifi_list_forget(&list, "b"));
    TEST_ASSERT_EQUAL_UINT8(2, list.count);
    TEST_ASSERT_EQUAL_STRING("c", list.networks[0].ssid);
    TEST_ASSERT_EQUAL_STRING("a", list.networks[1].ssid);
    TEST_ASSERT_TRUE(wifi_list_valid(&list));
    wifi_list_t damaged = list;
    damaged.count = 200;
    TEST_ASSERT_FALSE(wifi_list_forget(&damaged, "c"));
    TEST_ASSERT_TRUE(wifi_list_forget(&list, "c"));
    TEST_ASSERT_TRUE(wifi_list_forget(&list, "a"));
    const wifi_list_t empty = {0};
    TEST_ASSERT_EQUAL_MEMORY(&empty, &list, sizeof(list));
}

static void test_a_damaged_record_is_not_valid(void)
{
    wifi_list_t list = {0};
    TEST_ASSERT_TRUE(wifi_list_valid(&list));
    wifi_list_save(&list, "a", "password", NULL);

    wifi_list_t damaged = list;
    damaged.count = WIFI_LIST_CAPACITY + 1;
    TEST_ASSERT_FALSE(wifi_list_valid(&damaged));

    damaged = list;
    damaged.networks[3].ssid[0] = 'x';
    TEST_ASSERT_FALSE(wifi_list_valid(&damaged));

    damaged = list;
    memset(damaged.networks[0].ssid, 'a', sizeof(damaged.networks[0].ssid));
    TEST_ASSERT_FALSE(wifi_list_valid(&damaged));

    damaged = list;
    damaged.count = 2;
    damaged.networks[1] = damaged.networks[0];
    TEST_ASSERT_FALSE(wifi_list_valid(&damaged));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_ssid_is_one_to_32_bytes);
    RUN_TEST(test_password_is_open_text_or_hex_key);
    RUN_TEST(test_save_puts_new_and_updated_networks_first);
    RUN_TEST(test_a_sixth_network_replaces_the_oldest);
    RUN_TEST(test_an_invalid_network_leaves_the_list_unchanged);
    RUN_TEST(test_forget_removes_by_ssid_and_zeroes_the_slot);
    RUN_TEST(test_a_damaged_record_is_not_valid);
    return UNITY_END();
}
