#include <string.h>
#include "setup_credentials.h"
#include "setup_page.h"
#include "unity.h"

static char page[SETUP_PAGE_MAX_BYTES];

void setUp(void) {}
void tearDown(void) {}

static void test_the_form_carries_the_nonce_and_the_field_names_of_the_parser(void)
{
    const int length = setup_page_form(page, sizeof(page), "abc123");
    TEST_ASSERT_GREATER_THAN(0, length);
    TEST_ASSERT_EQUAL((int)strlen(page), length);
    TEST_ASSERT_NOT_NULL(strstr(page, "name=\"nonce\" value=\"abc123\""));
    TEST_ASSERT_NOT_NULL(strstr(page, "name=\"ssid\""));
    TEST_ASSERT_NOT_NULL(strstr(page, "name=\"password\""));
    TEST_ASSERT_NOT_NULL(strstr(page, "maxlength=\"32\""));
    TEST_ASSERT_NOT_NULL(strstr(page, "maxlength=\"64\""));
    TEST_ASSERT_NOT_NULL(strstr(page, "width=device-width"));
}

static void test_the_form_and_every_result_fit_the_page_buffer(void)
{
    char longest_ssid[WIFI_SSID_SIZE];
    memset(longest_ssid, '&', WIFI_SSID_MAX_BYTES);
    longest_ssid[WIFI_SSID_MAX_BYTES] = '\0';
    TEST_ASSERT_GREATER_THAN(0, setup_page_form(page, sizeof(page), "0123456789abcdef0123456789abcdef"));
    for (int result = SETUP_PAGE_SAVED; result <= SETUP_PAGE_FAILED; ++result) {
        TEST_ASSERT_GREATER_THAN(0, setup_page_result(page, sizeof(page), (setup_page_result_t)result, longest_ssid));
    }
}

static void test_a_page_that_does_not_fit_is_refused(void)
{
    char small[64];
    TEST_ASSERT_EQUAL(-1, setup_page_form(small, sizeof(small), "abc"));
    TEST_ASSERT_EQUAL(-1, setup_page_result(small, sizeof(small), SETUP_PAGE_SAVED, NULL));
    TEST_ASSERT_EQUAL(-1, setup_page_form(small, 0, "abc"));
}

static void test_the_replaced_network_name_is_escaped(void)
{
    TEST_ASSERT_GREATER_THAN(0, setup_page_result(page, sizeof(page), SETUP_PAGE_SAVED_REPLACED, "<b>\"x\" & 'y'</b>"));
    TEST_ASSERT_NOT_NULL(strstr(page, "&lt;b&gt;&quot;x&quot; &amp; &#39;y&#39;&lt;/b&gt;"));
    TEST_ASSERT_NULL(strstr(page, "<b>\"x\""));
}

static void test_a_plain_save_does_not_mention_a_replacement(void)
{
    TEST_ASSERT_GREATER_THAN(0, setup_page_result(page, sizeof(page), SETUP_PAGE_SAVED, "Old"));
    TEST_ASSERT_NOT_NULL(strstr(page, "Saved"));
    TEST_ASSERT_NULL(strstr(page, "replaced"));
    TEST_ASSERT_NULL(strstr(page, "Back to the form"));
}

static void test_errors_link_back_to_the_form(void)
{
    for (int result = SETUP_PAGE_INVALID; result <= SETUP_PAGE_FAILED; ++result) {
        TEST_ASSERT_GREATER_THAN(0, setup_page_result(page, sizeof(page), (setup_page_result_t)result, NULL));
        TEST_ASSERT_NOT_NULL(strstr(page, "href=\"/\""));
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_the_form_carries_the_nonce_and_the_field_names_of_the_parser);
    RUN_TEST(test_the_form_and_every_result_fit_the_page_buffer);
    RUN_TEST(test_a_page_that_does_not_fit_is_refused);
    RUN_TEST(test_the_replaced_network_name_is_escaped);
    RUN_TEST(test_a_plain_save_does_not_mention_a_replacement);
    RUN_TEST(test_errors_link_back_to_the_form);
    return UNITY_END();
}
