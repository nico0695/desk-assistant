#include "board_key_filter.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static board_key_filter_t filter;

static void test_stable_press_is_reported_once(void)
{
    board_key_filter_init(&filter, false, 0);
    TEST_ASSERT_FALSE(board_key_filter_update(&filter, false, 100));
    TEST_ASSERT_FALSE(board_key_filter_update(&filter, true, 110));
    TEST_ASSERT_FALSE(board_key_filter_update(&filter, true, 110 + BOARD_KEY_DEBOUNCE_MS - 1));
    TEST_ASSERT_TRUE(board_key_filter_update(&filter, true, 110 + BOARD_KEY_DEBOUNCE_MS));
    TEST_ASSERT_FALSE(board_key_filter_update(&filter, true, 1000));
}

static void test_short_release_bounce_is_not_a_new_press(void)
{
    board_key_filter_init(&filter, true, 0);
    TEST_ASSERT_FALSE(board_key_filter_update(&filter, false, 100));
    TEST_ASSERT_FALSE(board_key_filter_update(&filter, true, 105));
    TEST_ASSERT_FALSE(board_key_filter_update(&filter, true, 300));
}

static void test_settled_release_allows_the_next_press(void)
{
    board_key_filter_init(&filter, true, 0);
    TEST_ASSERT_FALSE(board_key_filter_update(&filter, false, 100));
    TEST_ASSERT_FALSE(board_key_filter_update(&filter, false, 100 + BOARD_KEY_DEBOUNCE_MS));
    TEST_ASSERT_FALSE(board_key_filter_update(&filter, true, 200));
    TEST_ASSERT_TRUE(board_key_filter_update(&filter, true, 200 + BOARD_KEY_DEBOUNCE_MS));
}

static void test_press_bounce_restarts_the_debounce(void)
{
    board_key_filter_init(&filter, false, 0);
    TEST_ASSERT_FALSE(board_key_filter_update(&filter, true, 10));
    TEST_ASSERT_FALSE(board_key_filter_update(&filter, false, 20));
    TEST_ASSERT_FALSE(board_key_filter_update(&filter, true, 25));
    TEST_ASSERT_FALSE(board_key_filter_update(&filter, true, 25 + BOARD_KEY_DEBOUNCE_MS - 1));
    TEST_ASSERT_TRUE(board_key_filter_update(&filter, true, 25 + BOARD_KEY_DEBOUNCE_MS));
}

static void test_key_held_at_start_is_not_a_press(void)
{
    board_key_filter_init(&filter, true, 10);
    TEST_ASSERT_FALSE(board_key_filter_update(&filter, true, 1000));
}

static void test_timestamps_may_wrap(void)
{
    const uint32_t start = UINT32_MAX - 10U;
    board_key_filter_init(&filter, false, start - 10U);
    TEST_ASSERT_FALSE(board_key_filter_update(&filter, true, start));
    TEST_ASSERT_FALSE(board_key_filter_update(&filter, true, start + BOARD_KEY_DEBOUNCE_MS - 1));
    TEST_ASSERT_TRUE(board_key_filter_update(&filter, true, start + BOARD_KEY_DEBOUNCE_MS));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_stable_press_is_reported_once);
    RUN_TEST(test_short_release_bounce_is_not_a_new_press);
    RUN_TEST(test_settled_release_allows_the_next_press);
    RUN_TEST(test_press_bounce_restarts_the_debounce);
    RUN_TEST(test_key_held_at_start_is_not_a_press);
    RUN_TEST(test_timestamps_may_wrap);
    return UNITY_END();
}
