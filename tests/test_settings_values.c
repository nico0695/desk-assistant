#include "settings_values.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static void test_only_the_listed_screen_timeouts_are_valid(void)
{
    for (unsigned i = 0; i < SETTINGS_SCREEN_TIMEOUT_OPTION_COUNT; ++i) {
        TEST_ASSERT_TRUE(settings_value_valid(SETTINGS_SCREEN_TIMEOUT, SETTINGS_SCREEN_TIMEOUT_OPTIONS_S[i]));
    }
    TEST_ASSERT_TRUE(settings_value_valid(SETTINGS_SCREEN_TIMEOUT, SETTINGS_SCREEN_TIMEOUT_ALWAYS_ON_S));
    TEST_ASSERT_FALSE(settings_value_valid(SETTINGS_SCREEN_TIMEOUT, 10));
    TEST_ASSERT_FALSE(settings_value_valid(SETTINGS_SCREEN_TIMEOUT, 31));
    TEST_ASSERT_FALSE(settings_value_valid(SETTINGS_SCREEN_TIMEOUT, 255));
}

static void test_percent_ranges_include_their_ends(void)
{
    TEST_ASSERT_TRUE(settings_value_valid(SETTINGS_VOLUME, 0));
    TEST_ASSERT_TRUE(settings_value_valid(SETTINGS_VOLUME, 100));
    TEST_ASSERT_FALSE(settings_value_valid(SETTINGS_VOLUME, 101));
    TEST_ASSERT_FALSE(settings_value_valid(SETTINGS_BRIGHTNESS, 9));
    TEST_ASSERT_TRUE(settings_value_valid(SETTINGS_BRIGHTNESS, 10));
    TEST_ASSERT_TRUE(settings_value_valid(SETTINGS_BRIGHTNESS, 100));
    TEST_ASSERT_FALSE(settings_value_valid(SETTINGS_BRIGHTNESS, 101));
    TEST_ASSERT_FALSE(settings_value_valid(SETTINGS_COUNT, 50));
}

static void test_defaults_are_valid_and_match_the_spec(void)
{
    TEST_ASSERT_EQUAL_UINT8(30, settings_default(SETTINGS_SCREEN_TIMEOUT));
    TEST_ASSERT_EQUAL_UINT8(25, settings_default(SETTINGS_VOLUME));
    TEST_ASSERT_EQUAL_UINT8(70, settings_default(SETTINGS_BRIGHTNESS));
    for (settings_id_t id = 0; id < SETTINGS_COUNT; ++id) {
        TEST_ASSERT_TRUE(settings_value_valid(id, settings_default(id)));
    }
}

static void test_a_stored_valid_value_is_used(void)
{
    settings_loaded_t loaded = {0};
    settings_resolve(&loaded, SETTINGS_BRIGHTNESS, SETTINGS_READ_OK, 40);
    TEST_ASSERT_EQUAL_UINT8(40, loaded.values[SETTINGS_BRIGHTNESS]);
    TEST_ASSERT_FALSE(loaded.load_failed[SETTINGS_BRIGHTNESS]);
}

static void test_a_missing_value_takes_the_default_without_error(void)
{
    settings_loaded_t loaded = {0};
    settings_resolve(&loaded, SETTINGS_VOLUME, SETTINGS_READ_MISSING, 0);
    TEST_ASSERT_EQUAL_UINT8(SETTINGS_VOLUME_DEFAULT_PERCENT, loaded.values[SETTINGS_VOLUME]);
    TEST_ASSERT_FALSE(loaded.load_failed[SETTINGS_VOLUME]);
}

static void test_an_invalid_or_unreadable_value_takes_the_default_with_error(void)
{
    settings_loaded_t loaded = {0};
    settings_resolve(&loaded, SETTINGS_SCREEN_TIMEOUT, SETTINGS_READ_OK, 45);
    TEST_ASSERT_EQUAL_UINT8(SETTINGS_SCREEN_TIMEOUT_DEFAULT_S, loaded.values[SETTINGS_SCREEN_TIMEOUT]);
    TEST_ASSERT_TRUE(loaded.load_failed[SETTINGS_SCREEN_TIMEOUT]);

    settings_resolve(&loaded, SETTINGS_BRIGHTNESS, SETTINGS_READ_FAILED, 50);
    TEST_ASSERT_EQUAL_UINT8(SETTINGS_BRIGHTNESS_DEFAULT_PERCENT, loaded.values[SETTINGS_BRIGHTNESS]);
    TEST_ASSERT_TRUE(loaded.load_failed[SETTINGS_BRIGHTNESS]);
    TEST_ASSERT_FALSE(loaded.load_failed[SETTINGS_VOLUME]);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_only_the_listed_screen_timeouts_are_valid);
    RUN_TEST(test_percent_ranges_include_their_ends);
    RUN_TEST(test_defaults_are_valid_and_match_the_spec);
    RUN_TEST(test_a_stored_valid_value_is_used);
    RUN_TEST(test_a_missing_value_takes_the_default_without_error);
    RUN_TEST(test_an_invalid_or_unreadable_value_takes_the_default_with_error);
    return UNITY_END();
}
