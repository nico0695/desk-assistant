#include "display_brightness.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static void test_known_levels_round_to_nearest(void)
{
    TEST_ASSERT_EQUAL_UINT8(0, display_brightness_panel_value(0));
    TEST_ASSERT_EQUAL_UINT8(26, display_brightness_panel_value(10));
    TEST_ASSERT_EQUAL_UINT8(128, display_brightness_panel_value(50));
    TEST_ASSERT_EQUAL_UINT8(179, display_brightness_panel_value(70));
    TEST_ASSERT_EQUAL_UINT8(255, display_brightness_panel_value(100));
}

static void test_values_above_maximum_are_clamped(void)
{
    TEST_ASSERT_EQUAL_UINT8(255, display_brightness_panel_value(101));
    TEST_ASSERT_EQUAL_UINT8(255, display_brightness_panel_value(UINT32_MAX));
}

static void test_each_percent_step_increases_the_panel_value(void)
{
    unsigned previous = display_brightness_panel_value(0);
    for (unsigned percent = 1; percent <= DISPLAY_BRIGHTNESS_MAX_PERCENT; ++percent) {
        const unsigned value = display_brightness_panel_value(percent);
        TEST_ASSERT_GREATER_THAN_UINT(previous, value);
        previous = value;
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_known_levels_round_to_nearest);
    RUN_TEST(test_values_above_maximum_are_clamped);
    RUN_TEST(test_each_percent_step_increases_the_panel_value);
    return UNITY_END();
}
