#include "touch_gate.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static touch_gate_t gate;

static void test_enabled_press_is_visible_and_reported(void)
{
    touch_gate_init(&gate);
    TEST_ASSERT_FALSE(touch_gate_take_activity(&gate));
    TEST_ASSERT_TRUE(touch_gate_sample(&gate, true, true));
    TEST_ASSERT_TRUE(touch_gate_take_activity(&gate));
    TEST_ASSERT_FALSE(touch_gate_take_activity(&gate));
    TEST_ASSERT_FALSE(touch_gate_sample(&gate, true, false));
}

static void test_wake_gesture_stays_hidden_until_a_valid_release(void)
{
    touch_gate_init(&gate);
    touch_gate_set_enabled(&gate, false);
    TEST_ASSERT_FALSE(touch_gate_sample(&gate, true, true));
    TEST_ASSERT_TRUE(touch_gate_take_activity(&gate));

    touch_gate_set_enabled(&gate, true);
    TEST_ASSERT_FALSE(touch_gate_sample(&gate, true, true));
    TEST_ASSERT_TRUE(touch_gate_held(&gate));
    TEST_ASSERT_FALSE(touch_gate_sample(&gate, false, false));
    TEST_ASSERT_TRUE(touch_gate_blocked(&gate));
    TEST_ASSERT_TRUE(touch_gate_held(&gate));
    TEST_ASSERT_FALSE(touch_gate_sample(&gate, true, true));
    TEST_ASSERT_FALSE(touch_gate_sample(&gate, true, false));
    TEST_ASSERT_FALSE(touch_gate_blocked(&gate));
    TEST_ASSERT_FALSE(touch_gate_held(&gate));
    TEST_ASSERT_TRUE(touch_gate_sample(&gate, true, true));
}

static void test_key_wake_without_a_press_allows_the_next_tap(void)
{
    touch_gate_init(&gate);
    touch_gate_set_enabled(&gate, false);
    TEST_ASSERT_FALSE(touch_gate_blocked(&gate));
    touch_gate_set_enabled(&gate, true);
    TEST_ASSERT_TRUE(touch_gate_sample(&gate, true, true));
}

static void test_hold_during_panel_off_does_not_wake_until_released(void)
{
    touch_gate_init(&gate);
    TEST_ASSERT_TRUE(touch_gate_sample(&gate, true, true));
    touch_gate_set_enabled(&gate, false);
    TEST_ASSERT_FALSE(touch_gate_take_activity(&gate));
    TEST_ASSERT_FALSE(touch_gate_sample(&gate, true, true));
    TEST_ASSERT_FALSE(touch_gate_take_activity(&gate));
    TEST_ASSERT_FALSE(touch_gate_sample(&gate, false, false));
    TEST_ASSERT_FALSE(touch_gate_take_activity(&gate));
    TEST_ASSERT_FALSE(touch_gate_sample(&gate, true, false));
    TEST_ASSERT_FALSE(touch_gate_sample(&gate, true, true));
    TEST_ASSERT_TRUE(touch_gate_take_activity(&gate));
}

static void test_invalid_samples_never_press_or_report_activity(void)
{
    touch_gate_init(&gate);
    TEST_ASSERT_FALSE(touch_gate_sample(&gate, false, true));
    TEST_ASSERT_FALSE(touch_gate_take_activity(&gate));
    TEST_ASSERT_FALSE(touch_gate_blocked(&gate));
}

static void test_bus_error_during_a_visible_press_cancels_it(void)
{
    touch_gate_init(&gate);
    TEST_ASSERT_TRUE(touch_gate_sample(&gate, true, true));
    TEST_ASSERT_FALSE(touch_gate_sample(&gate, false, false));
    TEST_ASSERT_TRUE(touch_gate_blocked(&gate));
    TEST_ASSERT_FALSE(touch_gate_sample(&gate, true, true));
    TEST_ASSERT_FALSE(touch_gate_sample(&gate, true, false));
    TEST_ASSERT_TRUE(touch_gate_sample(&gate, true, true));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_enabled_press_is_visible_and_reported);
    RUN_TEST(test_wake_gesture_stays_hidden_until_a_valid_release);
    RUN_TEST(test_key_wake_without_a_press_allows_the_next_tap);
    RUN_TEST(test_hold_during_panel_off_does_not_wake_until_released);
    RUN_TEST(test_invalid_samples_never_press_or_report_activity);
    RUN_TEST(test_bus_error_during_a_visible_press_cancels_it);
    return UNITY_END();
}
