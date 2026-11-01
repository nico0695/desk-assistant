#include "screen_power.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

#define TIMEOUT_MS (SCREEN_POWER_DEFAULT_TIMEOUT_S * 1000U)

static screen_power_t state;

static void test_inactivity_turns_the_panel_off_once(void)
{
    screen_power_init(&state, SCREEN_POWER_DEFAULT_TIMEOUT_S, 100);
    TEST_ASSERT_EQUAL(SCREEN_ACTION_NONE, screen_power_update(&state, 100 + TIMEOUT_MS - 1, 0));
    TEST_ASSERT_EQUAL(SCREEN_ACTION_SLEEP, screen_power_update(&state, 100 + TIMEOUT_MS, 0));
    TEST_ASSERT_TRUE(state.asleep);
    TEST_ASSERT_EQUAL(SCREEN_ACTION_NONE, screen_power_update(&state, 100 + TIMEOUT_MS + 1, 0));
}

static void test_held_touch_keeps_the_panel_on(void)
{
    screen_power_init(&state, SCREEN_POWER_DEFAULT_TIMEOUT_S, 0);
    for (uint64_t now = 0; now <= 3 * TIMEOUT_MS; now += 1000) {
        TEST_ASSERT_EQUAL(SCREEN_ACTION_NONE, screen_power_update(&state, now, SCREEN_EVENT_TOUCH));
    }
    TEST_ASSERT_EQUAL(SCREEN_ACTION_NONE, screen_power_update(&state, 4 * TIMEOUT_MS - 1, 0));
    TEST_ASSERT_EQUAL(SCREEN_ACTION_SLEEP, screen_power_update(&state, 4 * TIMEOUT_MS, 0));
}

static void test_touch_wakes_and_restarts_the_timeout(void)
{
    screen_power_init(&state, SCREEN_POWER_DEFAULT_TIMEOUT_S, 0);
    TEST_ASSERT_EQUAL(SCREEN_ACTION_SLEEP, screen_power_update(&state, TIMEOUT_MS, 0));
    TEST_ASSERT_EQUAL(SCREEN_ACTION_WAKE, screen_power_update(&state, TIMEOUT_MS + 100, SCREEN_EVENT_TOUCH));
    TEST_ASSERT_EQUAL_UINT64(TIMEOUT_MS + 100, state.last_activity_ms);
    TEST_ASSERT_EQUAL(SCREEN_ACTION_SLEEP, screen_power_update(&state, 2 * TIMEOUT_MS + 100, 0));
}

static void test_key_toggles_the_panel(void)
{
    screen_power_init(&state, SCREEN_POWER_DEFAULT_TIMEOUT_S, 0);
    TEST_ASSERT_EQUAL(SCREEN_ACTION_SLEEP, screen_power_update(&state, 1000, SCREEN_EVENT_KEY));
    TEST_ASSERT_TRUE(state.manual_sleep);
    TEST_ASSERT_EQUAL(SCREEN_ACTION_WAKE, screen_power_update(&state, 1001, SCREEN_EVENT_KEY));
    TEST_ASSERT_EQUAL_UINT64(1001, state.last_activity_ms);
}

static void test_always_on_never_times_out_but_key_still_toggles(void)
{
    screen_power_init(&state, SCREEN_POWER_ALWAYS_ON, 0);
    TEST_ASSERT_EQUAL(SCREEN_ACTION_NONE, screen_power_update(&state, UINT64_MAX, 0));
    TEST_ASSERT_EQUAL_UINT64(SCREEN_POWER_NO_DEADLINE, screen_power_next_deadline_ms(&state));
    TEST_ASSERT_EQUAL(SCREEN_ACTION_SLEEP, screen_power_update(&state, UINT64_MAX, SCREEN_EVENT_KEY));
    TEST_ASSERT_EQUAL(SCREEN_ACTION_WAKE, screen_power_update(&state, UINT64_MAX, SCREEN_EVENT_KEY));
}

static void test_rejected_wake_is_retried_without_a_new_event(void)
{
    const uint32_t causes[] = {SCREEN_EVENT_TOUCH, SCREEN_EVENT_KEY};
    for (size_t i = 0; i < sizeof(causes) / sizeof(causes[0]); ++i) {
        screen_power_init(&state, SCREEN_POWER_DEFAULT_TIMEOUT_S, 0);
        TEST_ASSERT_EQUAL(SCREEN_ACTION_SLEEP, screen_power_update(&state, TIMEOUT_MS, 0));
        const screen_power_t previous = state;
        TEST_ASSERT_EQUAL(SCREEN_ACTION_WAKE, screen_power_update(&state, TIMEOUT_MS + 1000, causes[i]));
        screen_power_reject_action(&state, &previous);
        TEST_ASSERT_TRUE(state.asleep && state.wake_pending);
        TEST_ASSERT_EQUAL_UINT64(state.last_activity_ms, screen_power_next_deadline_ms(&state));
        TEST_ASSERT_EQUAL(SCREEN_ACTION_WAKE, screen_power_update(&state, TIMEOUT_MS + 2000, 0));
        TEST_ASSERT_FALSE(state.asleep || state.wake_pending);
    }
}

static void test_rejected_manual_sleep_is_retried_until_cancelled(void)
{
    screen_power_init(&state, SCREEN_POWER_DEFAULT_TIMEOUT_S, 0);
    screen_power_t previous = state;
    TEST_ASSERT_EQUAL(SCREEN_ACTION_SLEEP, screen_power_update(&state, 100, SCREEN_EVENT_KEY));
    screen_power_reject_action(&state, &previous);
    TEST_ASSERT_TRUE(!state.asleep && state.sleep_pending);

    previous = state;
    TEST_ASSERT_EQUAL(SCREEN_ACTION_SLEEP, screen_power_update(&state, 110, 0));
    screen_power_reject_action(&state, &previous);
    TEST_ASSERT_TRUE(state.sleep_pending);

    TEST_ASSERT_EQUAL(SCREEN_ACTION_NONE, screen_power_update(&state, 120, SCREEN_EVENT_KEY));
    TEST_ASSERT_FALSE(state.asleep || state.sleep_pending);

    previous = state;
    TEST_ASSERT_EQUAL(SCREEN_ACTION_SLEEP, screen_power_update(&state, 130, SCREEN_EVENT_KEY));
    screen_power_reject_action(&state, &previous);
    TEST_ASSERT_EQUAL(SCREEN_ACTION_NONE, screen_power_update(&state, 140, SCREEN_EVENT_TOUCH));
    TEST_ASSERT_FALSE(state.sleep_pending);
}

static void test_rejected_timeout_sleep_retries_from_the_timeout(void)
{
    screen_power_init(&state, SCREEN_POWER_DEFAULT_TIMEOUT_S, 0);
    const screen_power_t previous = state;
    TEST_ASSERT_EQUAL(SCREEN_ACTION_SLEEP, screen_power_update(&state, TIMEOUT_MS, 0));
    screen_power_reject_action(&state, &previous);
    TEST_ASSERT_FALSE(state.asleep || state.sleep_pending);
    TEST_ASSERT_EQUAL(SCREEN_ACTION_SLEEP, screen_power_update(&state, TIMEOUT_MS + 1000, 0));
}

static void test_deadline_follows_the_last_activity(void)
{
    screen_power_init(&state, SCREEN_POWER_DEFAULT_TIMEOUT_S, 500);
    TEST_ASSERT_EQUAL_UINT64(500 + TIMEOUT_MS, screen_power_next_deadline_ms(&state));
    screen_power_update(&state, 2000, SCREEN_EVENT_TOUCH);
    TEST_ASSERT_EQUAL_UINT64(2000 + TIMEOUT_MS, screen_power_next_deadline_ms(&state));
    screen_power_update(&state, 2000 + TIMEOUT_MS, 0);
    TEST_ASSERT_EQUAL_UINT64(SCREEN_POWER_NO_DEADLINE, screen_power_next_deadline_ms(&state));
}

static void test_stale_and_large_timestamps_do_not_wrap(void)
{
    const uint64_t start = (uint64_t)UINT32_MAX + 1000;
    screen_power_init(&state, SCREEN_POWER_DEFAULT_TIMEOUT_S, start);
    TEST_ASSERT_EQUAL(SCREEN_ACTION_NONE, screen_power_update(&state, start - 1, 0));
    TEST_ASSERT_EQUAL(SCREEN_ACTION_NONE, screen_power_update(&state, start + TIMEOUT_MS - 1, 0));
    TEST_ASSERT_EQUAL(SCREEN_ACTION_SLEEP, screen_power_update(&state, start + TIMEOUT_MS, 0));
    TEST_ASSERT_EQUAL(SCREEN_ACTION_WAKE, screen_power_update(&state, start + TIMEOUT_MS + 1, SCREEN_EVENT_TOUCH));
    TEST_ASSERT_EQUAL(SCREEN_ACTION_NONE, screen_power_update(&state, start, SCREEN_EVENT_TOUCH));
    TEST_ASSERT_EQUAL_UINT64(start + TIMEOUT_MS + 1, state.last_activity_ms);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_inactivity_turns_the_panel_off_once);
    RUN_TEST(test_held_touch_keeps_the_panel_on);
    RUN_TEST(test_touch_wakes_and_restarts_the_timeout);
    RUN_TEST(test_key_toggles_the_panel);
    RUN_TEST(test_always_on_never_times_out_but_key_still_toggles);
    RUN_TEST(test_rejected_wake_is_retried_without_a_new_event);
    RUN_TEST(test_rejected_manual_sleep_is_retried_until_cancelled);
    RUN_TEST(test_rejected_timeout_sleep_retries_from_the_timeout);
    RUN_TEST(test_deadline_follows_the_last_activity);
    RUN_TEST(test_stale_and_large_timestamps_do_not_wrap);
    return UNITY_END();
}
