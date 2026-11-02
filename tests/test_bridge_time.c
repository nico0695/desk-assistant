#include <string.h>

#include "bridge_time.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static const char *local(const char *utc)
{
    static char text[BRIDGE_TIME_LOCAL_SIZE];
    TEST_ASSERT_TRUE(bridge_time_format_local(utc, text, sizeof(text)));
    return text;
}

static void test_formats_local_time_within_the_same_day(void)
{
    TEST_ASSERT_EQUAL_STRING("2026-11-02 09:00 UTC-3", local("2026-11-02T12:00:00Z"));
    TEST_ASSERT_EQUAL_STRING("2026-11-02 14:05 UTC-3", local("2026-11-02T17:05:59.123Z"));
    TEST_ASSERT_EQUAL_STRING("2026-11-02 00:00 UTC-3", local("2026-11-02T03:00:00Z"));
}

static void test_crosses_day_month_and_year_backwards(void)
{
    TEST_ASSERT_EQUAL_STRING("2026-11-01 21:30 UTC-3", local("2026-11-02T00:30:00Z"));
    TEST_ASSERT_EQUAL_STRING("2026-10-31 23:59 UTC-3", local("2026-11-01T02:59:00Z"));
    TEST_ASSERT_EQUAL_STRING("2025-12-31 21:05 UTC-3", local("2026-01-01T00:05:00Z"));
    TEST_ASSERT_EQUAL_STRING("2024-02-29 22:15 UTC-3", local("2024-03-01T01:15:00Z"));
    TEST_ASSERT_EQUAL_STRING("2025-02-28 22:15 UTC-3", local("2025-03-01T01:15:00Z"));
}

static void test_years_before_2010_keep_explicit_utc(void)
{
    TEST_ASSERT_EQUAL_STRING("2009-12-31 23:59 UTC", local("2009-12-31T23:59:00Z"));
    TEST_ASSERT_EQUAL_STRING("2010-01-01 00:00 UTC-3", local("2010-01-01T03:00:00Z"));
    TEST_ASSERT_EQUAL_STRING("2009-12-31 21:00 UTC-3", local("2010-01-01T00:00:00Z"));
}

static void test_rejects_invalid_text_and_small_buffers(void)
{
    char text[BRIDGE_TIME_LOCAL_SIZE] = "x";
    TEST_ASSERT_FALSE(bridge_time_format_local("2026-11-02 12:00:00", text, sizeof(text)));
    TEST_ASSERT_EQUAL_STRING("", text);
    TEST_ASSERT_FALSE(bridge_time_format_local("2026-13-02T12:00:00Z", text, sizeof(text)));
    TEST_ASSERT_FALSE(bridge_time_format_local("2026-11-02T12:00:00+03:00", text, sizeof(text)));
    TEST_ASSERT_FALSE(bridge_time_format_local(NULL, text, sizeof(text)));
    char small[22];
    TEST_ASSERT_FALSE(bridge_time_format_local("2026-11-02T12:00:00Z", small, sizeof(small)));
    TEST_ASSERT_EQUAL_STRING("", small);
    TEST_ASSERT_FALSE(bridge_time_format_local("2026-11-02T12:00:00Z", NULL, 10));
    TEST_ASSERT_FALSE(bridge_time_format_local("2026-11-02T12:00:00Z", text, 0));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_formats_local_time_within_the_same_day);
    RUN_TEST(test_crosses_day_month_and_year_backwards);
    RUN_TEST(test_years_before_2010_keep_explicit_utc);
    RUN_TEST(test_rejects_invalid_text_and_small_buffers);
    return UNITY_END();
}
