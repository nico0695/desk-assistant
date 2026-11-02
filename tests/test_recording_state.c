#include "recording_state.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static recording_state_t state;

/* Boot cleanup done: IDLE with its generation. */
static uint64_t start_idle(void)
{
    recording_state_init(&state);
    TEST_ASSERT_EQUAL(RECORDING_CANCELLING, state.phase);
    TEST_ASSERT_EQUAL(RECORDING_EFFECT_NONE, recording_state_record(&state, state.generation));
    recording_state_cleaned(&state, true);
    TEST_ASSERT_EQUAL(RECORDING_IDLE, state.phase);
    return state.generation;
}

static uint64_t start_recording(void)
{
    const uint64_t idle = start_idle();
    TEST_ASSERT_EQUAL(RECORDING_EFFECT_START_CAPTURE, recording_state_record(&state, idle));
    TEST_ASSERT_EQUAL(RECORDING_RECORDING, state.phase);
    return state.generation;
}

static void test_boot_cleanup_failure_blocks_capture_until_close_cleans(void)
{
    recording_state_init(&state);
    recording_state_cleaned(&state, false);
    TEST_ASSERT_EQUAL(RECORDING_FAILED, state.phase);
    TEST_ASSERT_EQUAL(RECORDING_FAILURE_CLEANUP, state.failure);
    TEST_ASSERT_EQUAL(RECORDING_EFFECT_NONE, recording_state_record(&state, state.generation));
    TEST_ASSERT_EQUAL(RECORDING_EFFECT_START_CLEANUP, recording_state_close(&state, state.generation));
    TEST_ASSERT_EQUAL(RECORDING_CANCELLING, state.phase);
    recording_state_cleaned(&state, true);
    TEST_ASSERT_EQUAL(RECORDING_IDLE, state.phase);
    TEST_ASSERT_EQUAL(RECORDING_FAILURE_NONE, state.failure);
}

static void test_send_finalizes_and_a_valid_clip_is_ready(void)
{
    const uint64_t attempt = start_recording();
    TEST_ASSERT_FALSE(recording_state_captured(&state, RECORDING_MIN_SAMPLES));
    TEST_ASSERT_EQUAL(RECORDING_EFFECT_NOTIFY_CAPTURE, recording_state_send(&state, attempt));
    TEST_ASSERT_EQUAL(RECORDING_FINALIZING, state.phase);
    TEST_ASSERT_EQUAL(RECORDING_EFFECT_NONE, recording_state_send(&state, attempt));
    TEST_ASSERT_EQUAL(RECORDING_EFFECT_NONE, recording_state_cancel(&state, attempt));
    TEST_ASSERT_EQUAL(RECORDING_FAILURE_NONE, recording_state_validate(&state, 0));
    recording_state_ready(&state);
    TEST_ASSERT_EQUAL(RECORDING_READY_TO_SEND, state.phase);

    recording_view_t view;
    recording_state_view(&state, &view);
    TEST_ASSERT_EQUAL(RECORDING_MIN_MS, view.duration_ms);
    TEST_ASSERT_EQUAL_UINT64(attempt, view.generation);
    /* No upload before stage 5b. */
    TEST_ASSERT_EQUAL(RECORDING_EFFECT_NONE, recording_state_send(&state, attempt));
}

static void test_natural_limit_finalizes_without_sending(void)
{
    start_recording();
    TEST_ASSERT_FALSE(recording_state_captured(&state, RECORDING_MAX_SAMPLES - 1U));
    TEST_ASSERT_TRUE(recording_state_captured(&state, RECORDING_MAX_SAMPLES));
    TEST_ASSERT_EQUAL(RECORDING_FINALIZING, state.phase);
    recording_state_ready(&state);
    TEST_ASSERT_EQUAL(RECORDING_READY_TO_SEND, state.phase);
}

static void test_short_or_lossy_capture_is_a_local_failure(void)
{
    start_recording();
    recording_state_captured(&state, RECORDING_MIN_SAMPLES - 1U);
    TEST_ASSERT_EQUAL(RECORDING_FAILURE_TOO_SHORT, recording_state_validate(&state, 0));
    recording_state_captured(&state, RECORDING_MAX_SAMPLES);
    TEST_ASSERT_EQUAL(RECORDING_FAILURE_INTERRUPTED, recording_state_validate(&state, 1));

    recording_state_failed(&state, RECORDING_FAILURE_TOO_SHORT, true);
    TEST_ASSERT_EQUAL(RECORDING_FAILED, state.phase);
    TEST_ASSERT_EQUAL(RECORDING_FAILURE_TOO_SHORT, state.failure);
    const uint64_t failed = state.generation;
    TEST_ASSERT_EQUAL(RECORDING_EFFECT_NONE, recording_state_close(&state, failed));
    TEST_ASSERT_EQUAL(RECORDING_IDLE, state.phase);
    TEST_ASSERT_NOT_EQUAL(failed, state.generation);
}

static void test_failed_cleanup_after_capture_failure_blocks_capture(void)
{
    start_recording();
    recording_state_failed(&state, RECORDING_FAILURE_INTERRUPTED, false);
    TEST_ASSERT_EQUAL(RECORDING_FAILURE_CLEANUP, state.failure);
    TEST_ASSERT_TRUE(state.cleanup_pending);
    TEST_ASSERT_EQUAL(RECORDING_EFFECT_START_CLEANUP, recording_state_close(&state, state.generation));
    recording_state_cleaned(&state, false);
    TEST_ASSERT_EQUAL(RECORDING_FAILED, state.phase);
    TEST_ASSERT_TRUE(state.cleanup_pending);
}

static void test_cancel_while_recording_or_ready(void)
{
    uint64_t attempt = start_recording();
    TEST_ASSERT_EQUAL(RECORDING_EFFECT_NOTIFY_CAPTURE, recording_state_cancel(&state, attempt));
    TEST_ASSERT_EQUAL(RECORDING_CANCELLING, state.phase);
    recording_state_cleaned(&state, true);
    TEST_ASSERT_EQUAL(RECORDING_IDLE, state.phase);

    TEST_ASSERT_EQUAL(RECORDING_EFFECT_START_CAPTURE, recording_state_record(&state, state.generation));
    attempt = state.generation;
    recording_state_captured(&state, RECORDING_MIN_SAMPLES);
    recording_state_send(&state, attempt);
    recording_state_ready(&state);
    TEST_ASSERT_EQUAL(RECORDING_EFFECT_START_CLEANUP, recording_state_cancel(&state, attempt));
    TEST_ASSERT_TRUE(state.cleanup_pending);
    recording_state_cleaned(&state, false);
    TEST_ASSERT_EQUAL(RECORDING_FAILED, state.phase);
    TEST_ASSERT_EQUAL(RECORDING_FAILURE_CLEANUP, state.failure);
}

static void test_double_tap_and_stale_commands_have_no_effect(void)
{
    const uint64_t idle = start_idle();
    TEST_ASSERT_EQUAL(RECORDING_EFFECT_START_CAPTURE, recording_state_record(&state, idle));
    const uint64_t attempt = state.generation;
    TEST_ASSERT_EQUAL(RECORDING_EFFECT_NONE, recording_state_record(&state, idle));
    TEST_ASSERT_EQUAL(RECORDING_EFFECT_NONE, recording_state_record(&state, attempt));
    TEST_ASSERT_EQUAL(RECORDING_EFFECT_NONE, recording_state_send(&state, idle));
    TEST_ASSERT_EQUAL(RECORDING_EFFECT_NONE, recording_state_cancel(&state, idle));
    TEST_ASSERT_EQUAL(RECORDING_RECORDING, state.phase);

    recording_state_cancel(&state, attempt);
    recording_state_cleaned(&state, true);
    TEST_ASSERT_EQUAL(RECORDING_EFFECT_NONE, recording_state_cancel(&state, attempt));
    TEST_ASSERT_EQUAL(RECORDING_EFFECT_NONE, recording_state_close(&state, attempt));
    /* A late cleanup outcome cannot change an attempt that already ended. */
    recording_state_cleaned(&state, false);
    TEST_ASSERT_EQUAL(RECORDING_IDLE, state.phase);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_boot_cleanup_failure_blocks_capture_until_close_cleans);
    RUN_TEST(test_send_finalizes_and_a_valid_clip_is_ready);
    RUN_TEST(test_natural_limit_finalizes_without_sending);
    RUN_TEST(test_short_or_lossy_capture_is_a_local_failure);
    RUN_TEST(test_failed_cleanup_after_capture_failure_blocks_capture);
    RUN_TEST(test_cancel_while_recording_or_ready);
    RUN_TEST(test_double_tap_and_stale_commands_have_no_effect);
    return UNITY_END();
}
