#include "display_sleep.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static void test_sleep_in_right_after_sleep_out_waits_the_full_settle(void)
{
    TEST_ASSERT_EQUAL_UINT32(DISPLAY_SLEEP_OUT_SETTLE_MS, display_sleep_in_wait_ms(1000, 1000));
}

static void test_sleep_in_waits_only_the_rest_of_the_settle(void)
{
    TEST_ASSERT_EQUAL_UINT32(DISPLAY_SLEEP_OUT_SETTLE_MS - 50U, display_sleep_in_wait_ms(1050, 1000));
    TEST_ASSERT_EQUAL_UINT32(1U, display_sleep_in_wait_ms(1000 + DISPLAY_SLEEP_OUT_SETTLE_MS - 1U, 1000));
}

static void test_sleep_in_after_the_settle_does_not_wait(void)
{
    TEST_ASSERT_EQUAL_UINT32(0U, display_sleep_in_wait_ms(1000 + DISPLAY_SLEEP_OUT_SETTLE_MS, 1000));
    TEST_ASSERT_EQUAL_UINT32(0U, display_sleep_in_wait_ms(UINT64_MAX, 0));
}

static void test_clock_before_the_sleep_out_waits_the_full_settle(void)
{
    TEST_ASSERT_EQUAL_UINT32(DISPLAY_SLEEP_OUT_SETTLE_MS, display_sleep_in_wait_ms(900, 1000));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_sleep_in_right_after_sleep_out_waits_the_full_settle);
    RUN_TEST(test_sleep_in_waits_only_the_rest_of_the_settle);
    RUN_TEST(test_sleep_in_after_the_settle_does_not_wait);
    RUN_TEST(test_clock_before_the_sleep_out_waits_the_full_settle);
    return UNITY_END();
}
