#include "settings_row.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static settings_row_t row;

static void drag_and_apply(uint8_t value, bool ok)
{
    const settings_row_effect_t effect = settings_row_drag(&row, value);
    TEST_ASSERT_TRUE(effect.apply);
    TEST_ASSERT_EQUAL_UINT8(value, effect.value);
    settings_row_apply_result(&row, value, ok);
}

static void test_brightness_applies_while_dragging_and_saves_once_on_release(void)
{
    settings_row_init(&row, SETTINGS_BRIGHTNESS, 70, false);
    drag_and_apply(60, true);
    drag_and_apply(50, true);
    TEST_ASSERT_FALSE(settings_row_drag(&row, 50).apply);
    TEST_ASSERT_EQUAL_UINT8(70, row.confirmed);

    const settings_row_effect_t effect = settings_row_release(&row);
    TEST_ASSERT_TRUE(effect.save);
    TEST_ASSERT_FALSE(effect.apply);
    TEST_ASSERT_EQUAL_UINT8(50, effect.value);
    TEST_ASSERT_EQUAL_UINT8(50, row.confirmed);
    TEST_ASSERT_FALSE(settings_row_release(&row).save);
}

static void test_release_without_change_does_not_save(void)
{
    settings_row_init(&row, SETTINGS_BRIGHTNESS, 70, false);
    drag_and_apply(60, true);
    drag_and_apply(70, true);
    const settings_row_effect_t effect = settings_row_release(&row);
    TEST_ASSERT_FALSE(effect.save);
    TEST_ASSERT_FALSE(effect.apply);
}

static void test_a_failed_apply_is_not_saved_as_applied(void)
{
    settings_row_init(&row, SETTINGS_BRIGHTNESS, 70, false);
    drag_and_apply(50, true);
    drag_and_apply(30, false);
    TEST_ASSERT_EQUAL(SETTINGS_ROW_ERROR_APPLY, settings_row_error(&row));

    const settings_row_effect_t effect = settings_row_release(&row);
    TEST_ASSERT_TRUE(effect.save);
    TEST_ASSERT_EQUAL_UINT8(50, effect.value);
    TEST_ASSERT_EQUAL_UINT8(50, row.shown);
    TEST_ASSERT_EQUAL(SETTINGS_ROW_ERROR_NONE, settings_row_error(&row));
}

static void test_cancel_mid_drag_restores_and_reapplies_the_confirmed_brightness(void)
{
    settings_row_init(&row, SETTINGS_BRIGHTNESS, 70, false);
    drag_and_apply(20, true);
    const settings_row_effect_t effect = settings_row_cancel(&row);
    TEST_ASSERT_TRUE(effect.apply);
    TEST_ASSERT_FALSE(effect.save);
    TEST_ASSERT_EQUAL_UINT8(70, effect.value);
    TEST_ASSERT_EQUAL_UINT8(70, row.shown);
    TEST_ASSERT_FALSE(row.dragging);
    TEST_ASSERT_FALSE(settings_row_cancel(&row).apply);
}

static void test_volume_only_shows_while_dragging_and_saves_on_release(void)
{
    settings_row_init(&row, SETTINGS_VOLUME, 25, false);
    TEST_ASSERT_FALSE(settings_row_drag(&row, 0).apply);
    TEST_ASSERT_EQUAL_UINT8(0, row.shown);
    TEST_ASSERT_EQUAL_UINT8(25, row.confirmed);

    const settings_row_effect_t effect = settings_row_release(&row);
    TEST_ASSERT_TRUE(effect.save);
    TEST_ASSERT_EQUAL_UINT8(0, effect.value);
}

static void test_cancelled_volume_shows_the_confirmed_value_without_effects(void)
{
    settings_row_init(&row, SETTINGS_VOLUME, 25, false);
    settings_row_drag(&row, 90);
    const settings_row_effect_t effect = settings_row_cancel(&row);
    TEST_ASSERT_FALSE(effect.apply);
    TEST_ASSERT_FALSE(effect.save);
    TEST_ASSERT_EQUAL_UINT8(25, row.shown);
}

static void test_out_of_range_drag_is_ignored(void)
{
    settings_row_init(&row, SETTINGS_BRIGHTNESS, 70, false);
    TEST_ASSERT_FALSE(settings_row_drag(&row, 5).apply);
    TEST_ASSERT_FALSE(row.dragging);
    TEST_ASSERT_EQUAL_UINT8(70, row.shown);
}

static void test_select_applies_and_saves_a_new_timeout(void)
{
    settings_row_init(&row, SETTINGS_SCREEN_TIMEOUT, 30, false);
    TEST_ASSERT_FALSE(settings_row_select(&row, 30).save);
    TEST_ASSERT_FALSE(settings_row_select(&row, 45).save);

    const settings_row_effect_t effect = settings_row_select(&row, SETTINGS_SCREEN_TIMEOUT_ALWAYS_ON_S);
    TEST_ASSERT_TRUE(effect.apply);
    TEST_ASSERT_TRUE(effect.save);
    TEST_ASSERT_EQUAL_UINT8(SETTINGS_SCREEN_TIMEOUT_ALWAYS_ON_S, row.confirmed);
}

static void test_only_the_latest_save_result_counts(void)
{
    settings_row_init(&row, SETTINGS_SCREEN_TIMEOUT, 30, false);
    const uint32_t first = settings_row_select(&row, 15).save_sequence;
    const uint32_t second = settings_row_select(&row, 60).save_sequence;
    TEST_ASSERT_NOT_EQUAL(first, second);

    settings_row_save_result(&row, second, false);
    settings_row_save_result(&row, first, true);
    TEST_ASSERT_EQUAL(SETTINGS_ROW_ERROR_SAVE, settings_row_error(&row));

    settings_row_save_result(&row, second, true);
    TEST_ASSERT_EQUAL(SETTINGS_ROW_ERROR_NONE, settings_row_error(&row));
}

static void test_a_successful_save_clears_a_load_error(void)
{
    settings_row_init(&row, SETTINGS_VOLUME, SETTINGS_VOLUME_DEFAULT_PERCENT, true);
    TEST_ASSERT_EQUAL(SETTINGS_ROW_ERROR_LOAD, settings_row_error(&row));
    settings_row_drag(&row, 40);
    const settings_row_effect_t effect = settings_row_release(&row);
    settings_row_save_result(&row, effect.save_sequence, true);
    TEST_ASSERT_EQUAL(SETTINGS_ROW_ERROR_NONE, settings_row_error(&row));
}

static void test_a_save_result_does_not_hide_a_current_apply_failure(void)
{
    settings_row_init(&row, SETTINGS_BRIGHTNESS, 70, false);
    drag_and_apply(50, true);
    const settings_row_effect_t effect = settings_row_release(&row);
    drag_and_apply(40, false);
    settings_row_save_result(&row, effect.save_sequence, true);
    TEST_ASSERT_EQUAL(SETTINGS_ROW_ERROR_APPLY, settings_row_error(&row));
}

static void test_returning_to_the_applied_brightness_clears_the_apply_error(void)
{
    settings_row_init(&row, SETTINGS_BRIGHTNESS, 70, false);
    drag_and_apply(50, true);
    drag_and_apply(30, false);
    TEST_ASSERT_FALSE(settings_row_drag(&row, 50).apply);
    TEST_ASSERT_EQUAL(SETTINGS_ROW_ERROR_NONE, settings_row_error(&row));
}

static void test_cancel_after_a_failed_apply_clears_the_error_when_the_panel_kept_the_confirmed_value(void)
{
    settings_row_init(&row, SETTINGS_BRIGHTNESS, 70, false);
    drag_and_apply(30, false);
    TEST_ASSERT_FALSE(settings_row_cancel(&row).apply);
    TEST_ASSERT_EQUAL(SETTINGS_ROW_ERROR_NONE, settings_row_error(&row));
}

static void test_an_external_value_is_confirmed_and_makes_a_pending_save_stale(void)
{
    settings_row_init(&row, SETTINGS_SCREEN_TIMEOUT, 30, true);
    const settings_row_effect_t selected = settings_row_select(&row, 15);
    const settings_row_effect_t external = settings_row_external(&row, 60);
    TEST_ASSERT_TRUE(external.apply);
    TEST_ASSERT_FALSE(external.save);
    TEST_ASSERT_EQUAL_UINT8(60, external.value);
    settings_row_apply_result(&row, 60, true);
    TEST_ASSERT_EQUAL_UINT8(60, row.confirmed);
    TEST_ASSERT_EQUAL(SETTINGS_ROW_ERROR_NONE, settings_row_error(&row));
    /* The UI save that was still in flight fails late: it no longer describes the row. */
    settings_row_save_result(&row, selected.save_sequence, false);
    TEST_ASSERT_EQUAL(SETTINGS_ROW_ERROR_NONE, settings_row_error(&row));
    TEST_ASSERT_FALSE(settings_row_external(&row, 45).apply);
    TEST_ASSERT_EQUAL_UINT8(60, row.confirmed);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_brightness_applies_while_dragging_and_saves_once_on_release);
    RUN_TEST(test_release_without_change_does_not_save);
    RUN_TEST(test_a_failed_apply_is_not_saved_as_applied);
    RUN_TEST(test_cancel_mid_drag_restores_and_reapplies_the_confirmed_brightness);
    RUN_TEST(test_volume_only_shows_while_dragging_and_saves_on_release);
    RUN_TEST(test_cancelled_volume_shows_the_confirmed_value_without_effects);
    RUN_TEST(test_out_of_range_drag_is_ignored);
    RUN_TEST(test_select_applies_and_saves_a_new_timeout);
    RUN_TEST(test_only_the_latest_save_result_counts);
    RUN_TEST(test_a_successful_save_clears_a_load_error);
    RUN_TEST(test_a_save_result_does_not_hide_a_current_apply_failure);
    RUN_TEST(test_returning_to_the_applied_brightness_clears_the_apply_error);
    RUN_TEST(test_cancel_after_a_failed_apply_clears_the_error_when_the_panel_kept_the_confirmed_value);
    RUN_TEST(test_an_external_value_is_confirmed_and_makes_a_pending_save_stale);
    return UNITY_END();
}
