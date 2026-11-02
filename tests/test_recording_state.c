#include "recording_state.h"
#include "unity.h"

#define ID "12345678-1234-4abc-9876-123456789abc"
#define OTHER_ID "87654321-4321-4cba-8678-cba987654321"

void setUp(void) {}
void tearDown(void) {}

static recording_state_t state;

static recording_view_t view(void)
{
    recording_view_t result;
    recording_state_view(&state, &result);
    return result;
}

/* Boot cleanup done: IDLE with its generation. */
static uint64_t start_idle(void)
{
    recording_state_init(&state);
    TEST_ASSERT_EQUAL(RECORDING_CANCELLING, state.phase);
    TEST_ASSERT_EQUAL(RECORDING_EFFECT_NONE, recording_state_record(&state, state.generation));
    TEST_ASSERT_EQUAL(RECORDING_JOB_CLEANUP, recording_state_claim_job(&state));
    recording_state_cleaned(&state, true);
    TEST_ASSERT_EQUAL(RECORDING_JOB_NONE, recording_state_next_job(&state));
    TEST_ASSERT_EQUAL(RECORDING_IDLE, state.phase);
    return state.generation;
}

static uint64_t start_recording(void)
{
    const uint64_t idle = start_idle();
    TEST_ASSERT_EQUAL(RECORDING_EFFECT_VIEW, recording_state_record(&state, idle));
    TEST_ASSERT_EQUAL(RECORDING_RECORDING, state.phase);
    TEST_ASSERT_EQUAL(RECORDING_JOB_CAPTURE, recording_state_claim_job(&state));
    return state.generation;
}

/* A valid clip finalized with Send continues to the upload in the capture job. */
static void start_upload(void)
{
    const uint64_t attempt = start_recording();
    recording_state_captured(&state, RECORDING_MIN_SAMPLES);
    TEST_ASSERT_EQUAL(RECORDING_EFFECT_NOTIFY_CAPTURE, recording_state_send(&state, attempt));
    TEST_ASSERT_EQUAL(RECORDING_FAILURE_NONE, recording_state_validate(&state, 0));
    TEST_ASSERT_TRUE(recording_state_ready(&state));
    TEST_ASSERT_EQUAL(RECORDING_UPLOADING, state.phase);
}

static void start_reserved_upload(void)
{
    start_upload();
    recording_state_reserved(&state, ID, true);
    recording_state_cleaned(&state, true);
}

static void start_processing(void)
{
    start_reserved_upload();
    TEST_ASSERT_EQUAL(RECORDING_APPLY_WAIT, recording_state_reply(&state, ID, RECORDING_REPLY_PROCESSING));
    TEST_ASSERT_EQUAL(RECORDING_JOB_QUERY, recording_state_next_job(&state));
    TEST_ASSERT_EQUAL(RECORDING_JOB_NONE, recording_state_next_job(&state));
}

static void test_boot_cleanup_failure_blocks_capture_until_close_cleans(void)
{
    recording_state_init(&state);
    recording_state_claim_job(&state);
    recording_state_cleaned(&state, false);
    recording_state_next_job(&state);
    TEST_ASSERT_EQUAL(RECORDING_FAILED, state.phase);
    TEST_ASSERT_EQUAL(RECORDING_FAILURE_CLEANUP, state.failure);
    TEST_ASSERT_EQUAL(RECORDING_EFFECT_NONE, recording_state_record(&state, state.generation));
    TEST_ASSERT_EQUAL(RECORDING_EFFECT_VIEW, recording_state_close(&state, state.generation));
    TEST_ASSERT_EQUAL(RECORDING_CANCELLING, state.phase);
    TEST_ASSERT_EQUAL(RECORDING_JOB_CLEANUP, recording_state_claim_job(&state));
    recording_state_cleaned(&state, true);
    TEST_ASSERT_EQUAL(RECORDING_IDLE, state.phase);
    TEST_ASSERT_EQUAL(RECORDING_FAILURE_NONE, state.failure);
}

static void test_natural_limit_finalizes_without_sending(void)
{
    start_recording();
    TEST_ASSERT_FALSE(recording_state_captured(&state, RECORDING_MAX_SAMPLES - 1U));
    TEST_ASSERT_TRUE(recording_state_captured(&state, RECORDING_MAX_SAMPLES));
    TEST_ASSERT_EQUAL(RECORDING_FINALIZING, state.phase);
    TEST_ASSERT_FALSE(recording_state_ready(&state));
    TEST_ASSERT_EQUAL(RECORDING_READY_TO_SEND, state.phase);
    TEST_ASSERT_EQUAL(RECORDING_MAX_MS, view().duration_ms);
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
    TEST_ASSERT_EQUAL(RECORDING_EFFECT_VIEW, recording_state_close(&state, failed));
    TEST_ASSERT_EQUAL(RECORDING_IDLE, state.phase);
    TEST_ASSERT_NOT_EQUAL(failed, state.generation);
}

static void test_failed_cleanup_after_capture_failure_blocks_capture(void)
{
    start_recording();
    recording_state_failed(&state, RECORDING_FAILURE_INTERRUPTED, false);
    TEST_ASSERT_EQUAL(RECORDING_FAILURE_CLEANUP, state.failure);
    TEST_ASSERT_TRUE(state.cleanup_pending);
    recording_state_next_job(&state);
    TEST_ASSERT_EQUAL(RECORDING_EFFECT_VIEW, recording_state_close(&state, state.generation));
    TEST_ASSERT_EQUAL(RECORDING_JOB_CLEANUP, recording_state_claim_job(&state));
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
    recording_state_next_job(&state);

    TEST_ASSERT_EQUAL(RECORDING_EFFECT_VIEW, recording_state_record(&state, state.generation));
    recording_state_claim_job(&state);
    attempt = state.generation;
    recording_state_captured(&state, RECORDING_MIN_SAMPLES);
    TEST_ASSERT_TRUE(recording_state_captured(&state, RECORDING_MAX_SAMPLES));
    recording_state_ready(&state);
    recording_state_next_job(&state);
    TEST_ASSERT_EQUAL(RECORDING_EFFECT_VIEW, recording_state_cancel(&state, attempt));
    TEST_ASSERT_TRUE(state.cleanup_pending);
    TEST_ASSERT_EQUAL(RECORDING_JOB_CLEANUP, recording_state_claim_job(&state));
    recording_state_cleaned(&state, false);
    TEST_ASSERT_EQUAL(RECORDING_FAILED, state.phase);
    TEST_ASSERT_EQUAL(RECORDING_FAILURE_CLEANUP, state.failure);
}

static void test_double_tap_and_stale_commands_have_no_effect(void)
{
    const uint64_t idle = start_idle();
    TEST_ASSERT_EQUAL(RECORDING_EFFECT_VIEW, recording_state_record(&state, idle));
    const uint64_t attempt = state.generation;
    TEST_ASSERT_EQUAL(RECORDING_EFFECT_NONE, recording_state_record(&state, idle));
    TEST_ASSERT_EQUAL(RECORDING_EFFECT_NONE, recording_state_record(&state, attempt));
    TEST_ASSERT_EQUAL(RECORDING_EFFECT_NONE, recording_state_send(&state, idle));
    TEST_ASSERT_EQUAL(RECORDING_EFFECT_NONE, recording_state_cancel(&state, idle));
    TEST_ASSERT_EQUAL(RECORDING_RECORDING, state.phase);
    recording_state_captured(&state, RECORDING_MIN_SAMPLES);
    TEST_ASSERT_EQUAL(RECORDING_EFFECT_NOTIFY_CAPTURE, recording_state_send(&state, attempt));
    TEST_ASSERT_EQUAL(RECORDING_EFFECT_NONE, recording_state_send(&state, attempt));
    TEST_ASSERT_EQUAL(RECORDING_EFFECT_NONE, recording_state_cancel(&state, attempt));
    TEST_ASSERT_TRUE(recording_state_ready(&state));
    TEST_ASSERT_EQUAL(RECORDING_EFFECT_NONE, recording_state_send(&state, attempt));
    TEST_ASSERT_EQUAL(RECORDING_JOB_NONE, recording_state_claim_job(&state));
    /* Signals before the identity exists, and a stale cleanup outcome in IDLE, change nothing. */
    TEST_ASSERT_EQUAL(RECORDING_EFFECT_NONE, recording_state_signal(&state, NULL));
    recording_state_init(&state);
    state.phase = RECORDING_IDLE;
    recording_state_cleaned(&state, false);
    TEST_ASSERT_EQUAL(RECORDING_IDLE, state.phase);
}

static void test_send_from_ready_without_channel_keeps_the_clip(void)
{
    start_recording();
    TEST_ASSERT_TRUE(recording_state_captured(&state, RECORDING_MAX_SAMPLES));
    recording_state_ready(&state);
    recording_state_next_job(&state);
    const uint64_t ready = state.generation;
    TEST_ASSERT_EQUAL(RECORDING_EFFECT_VIEW, recording_state_send(&state, ready));
    TEST_ASSERT_EQUAL(RECORDING_UPLOADING, state.phase);
    TEST_ASSERT_EQUAL(RECORDING_JOB_UPLOAD, recording_state_claim_job(&state));
    recording_state_refused(&state);
    recording_state_next_job(&state);
    TEST_ASSERT_EQUAL(RECORDING_READY_TO_SEND, state.phase);
    TEST_ASSERT_EQUAL(RECORDING_NOTICE_NOT_CONNECTED, view().notice);
    TEST_ASSERT_FALSE(state.cleanup_pending);
    TEST_ASSERT_EQUAL(RECORDING_EFFECT_VIEW, recording_state_send(&state, ready));
    TEST_ASSERT_EQUAL(RECORDING_NOTICE_NONE, view().notice);
}

static void test_failed_reservation_blocks_everything(void)
{
    start_upload();
    recording_state_reserved(&state, ID, false);
    recording_state_cleaned(&state, true);
    TEST_ASSERT_EQUAL(RECORDING_FAILED, state.phase);
    TEST_ASSERT_EQUAL(RECORDING_FAILURE_RECORD, state.failure);
    TEST_ASSERT_TRUE(view().close_keeps_attempt);
    TEST_ASSERT_FALSE(view().can_check);
    TEST_ASSERT_EQUAL(RECORDING_EFFECT_NONE, recording_state_close(&state, state.generation));
    TEST_ASSERT_EQUAL(RECORDING_APPLY_IGNORE, recording_state_reply(&state, ID, RECORDING_REPLY_COMPLETED));
    recording_state_signal(&state, ID);
    TEST_ASSERT_EQUAL(RECORDING_JOB_NONE, recording_state_next_job(&state));
}

static void test_unsent_upload_is_not_sent_only_when_forgotten(void)
{
    start_reserved_upload();
    const uint64_t uploading = state.generation;
    recording_state_upload_failed(&state, ID, RECORDING_UPLOAD_UNSENT);
    TEST_ASSERT_EQUAL(RECORDING_FAILED, state.phase);
    TEST_ASSERT_EQUAL(RECORDING_FAILURE_NOT_SENT, state.failure);
    TEST_ASSERT_EQUAL_STRING("", state.id);
    TEST_ASSERT_EQUAL(RECORDING_JOB_NONE, recording_state_next_job(&state));
    TEST_ASSERT_EQUAL(RECORDING_EFFECT_NONE, recording_state_close(&state, uploading));
    TEST_ASSERT_EQUAL(RECORDING_EFFECT_VIEW, recording_state_close(&state, state.generation));
    TEST_ASSERT_EQUAL(RECORDING_IDLE, state.phase);

    start_upload();
    recording_state_reserved(&state, ID, true);
    recording_state_cleaned(&state, false);
    recording_state_upload_failed(&state, ID, RECORDING_UPLOAD_UNSENT);
    TEST_ASSERT_EQUAL(RECORDING_FAILURE_CLEANUP, state.failure);
    TEST_ASSERT_TRUE(state.cleanup_pending);

    start_reserved_upload();
    recording_state_upload_failed(&state, ID, RECORDING_UPLOAD_UNSENT_KEPT);
    TEST_ASSERT_EQUAL(RECORDING_UNCONFIRMED, state.phase);
    TEST_ASSERT_EQUAL(RECORDING_FAILURE_RECORD, state.failure);
    TEST_ASSERT_TRUE(view().can_check);
    TEST_ASSERT_FALSE(view().can_discard);
    TEST_ASSERT_TRUE(view().close_keeps_attempt);
    TEST_ASSERT_EQUAL(RECORDING_EFFECT_NONE, recording_state_discard(&state, state.generation));
}

static void test_ambiguous_upload_is_unconfirmed_until_a_terminal_reply(void)
{
    start_reserved_upload();
    recording_state_upload_failed(&state, ID, RECORDING_UPLOAD_AMBIGUOUS);
    TEST_ASSERT_EQUAL(RECORDING_UNCONFIRMED, state.phase);
    TEST_ASSERT_TRUE(view().can_discard);
    TEST_ASSERT_EQUAL(RECORDING_JOB_QUERY, recording_state_next_job(&state));
    TEST_ASSERT_EQUAL(RECORDING_APPLY_IGNORE, recording_state_reply(&state, ID, RECORDING_REPLY_PROCESSING));
    TEST_ASSERT_EQUAL(RECORDING_APPLY_IGNORE, recording_state_reply(&state, OTHER_ID, RECORDING_REPLY_COMPLETED));
    TEST_ASSERT_EQUAL(RECORDING_APPLY_STORE, recording_state_reply(&state, ID, RECORDING_REPLY_COMPLETED));
    recording_state_resolved(&state, ID, RECORDING_REPLY_COMPLETED, true);
    TEST_ASSERT_EQUAL(RECORDING_COMPLETED, state.phase);
    TEST_ASSERT_TRUE(view().has_message);
    TEST_ASSERT_TRUE(view().content_omitted);
    TEST_ASSERT_FALSE(view().can_check);
    TEST_ASSERT_FALSE(view().close_keeps_attempt);
}

static void test_late_or_foreign_replies_do_not_change_a_terminal(void)
{
    start_processing();
    TEST_ASSERT_EQUAL(RECORDING_APPLY_STORE, recording_state_reply(&state, ID, RECORDING_REPLY_FAILED));
    recording_state_resolved(&state, ID, RECORDING_REPLY_FAILED, false);
    TEST_ASSERT_EQUAL(RECORDING_FAILED, state.phase);
    TEST_ASSERT_EQUAL(RECORDING_FAILURE_REMOTE, state.failure);
    const uint64_t terminal = state.generation;
    TEST_ASSERT_EQUAL(RECORDING_APPLY_IGNORE, recording_state_reply(&state, ID, RECORDING_REPLY_PROCESSING));
    TEST_ASSERT_EQUAL(RECORDING_APPLY_IGNORE, recording_state_reply(&state, ID, RECORDING_REPLY_COMPLETED));
    TEST_ASSERT_EQUAL(RECORDING_APPLY_IGNORE, recording_state_reply(&state, ID, RECORDING_REPLY_FAILED));
    recording_state_resolved(&state, ID, RECORDING_REPLY_COMPLETED, false);
    recording_state_signal(&state, ID);
    TEST_ASSERT_EQUAL(RECORDING_JOB_NONE, recording_state_claim_job(&state));
    TEST_ASSERT_EQUAL(RECORDING_FAILED, state.phase);
    TEST_ASSERT_EQUAL_UINT64(terminal, state.generation);
}

static void test_local_wait_marks_unconfirmed_only_while_processing(void)
{
    start_processing();
    const uint64_t processing = state.generation;
    TEST_ASSERT_EQUAL(RECORDING_EFFECT_NONE, recording_state_wait_expired(&state, processing - 1U));
    TEST_ASSERT_FALSE(view().can_discard);
    TEST_ASSERT_TRUE(view().close_keeps_attempt);
    TEST_ASSERT_EQUAL(RECORDING_EFFECT_VIEW, recording_state_wait_expired(&state, processing));
    TEST_ASSERT_EQUAL(RECORDING_UNCONFIRMED, state.phase);
    TEST_ASSERT_EQUAL(RECORDING_JOB_QUERY, recording_state_claim_job(&state));

    start_processing();
    const uint64_t pending = state.generation;
    recording_state_resolved(&state, ID, RECORDING_REPLY_UNKNOWN, false);
    TEST_ASSERT_EQUAL(RECORDING_EFFECT_NONE, recording_state_wait_expired(&state, pending));
    TEST_ASSERT_EQUAL(RECORDING_EFFECT_NONE, recording_state_wait_expired(&state, state.generation));
    TEST_ASSERT_EQUAL(RECORDING_UNKNOWN, state.phase);
}

static void test_restore_resumes_pending_and_terminal_attempts(void)
{
    recording_state_init(&state);
    recording_state_restore(&state, ID, RECORDING_REPLY_PROCESSING);
    TEST_ASSERT_EQUAL(RECORDING_PROCESSING, state.phase);
    TEST_ASSERT_EQUAL(RECORDING_JOB_CLEANUP, recording_state_claim_job(&state));
    recording_state_cleaned(&state, true);
    TEST_ASSERT_EQUAL(RECORDING_PROCESSING, state.phase);
    TEST_ASSERT_EQUAL(RECORDING_JOB_QUERY, recording_state_next_job(&state));
    TEST_ASSERT_EQUAL(RECORDING_EFFECT_VIEW, recording_state_wait_expired(&state, state.generation));
    TEST_ASSERT_TRUE(view().can_discard);

    recording_state_init(&state);
    recording_state_restore(&state, ID, RECORDING_REPLY_COMPLETED);
    TEST_ASSERT_EQUAL(RECORDING_COMPLETED, state.phase);
    TEST_ASSERT_FALSE(view().has_message);
    TEST_ASSERT_TRUE(view().can_check);
    recording_state_claim_job(&state);
    recording_state_cleaned(&state, true);
    TEST_ASSERT_EQUAL(RECORDING_JOB_QUERY, recording_state_next_job(&state));
    const uint64_t without_text = state.generation;
    TEST_ASSERT_EQUAL(RECORDING_APPLY_IGNORE, recording_state_reply(&state, ID, RECORDING_REPLY_FAILED));
    TEST_ASSERT_EQUAL(RECORDING_APPLY_SHOW, recording_state_reply(&state, ID, RECORDING_REPLY_COMPLETED));
    recording_state_resolved(&state, ID, RECORDING_REPLY_COMPLETED, false);
    TEST_ASSERT_TRUE(view().has_message);
    TEST_ASSERT_NOT_EQUAL(without_text, state.generation);

    recording_state_init(&state);
    recording_state_restore_failed(&state);
    TEST_ASSERT_EQUAL(RECORDING_FAILURE_RECORD, state.failure);
    TEST_ASSERT_EQUAL(RECORDING_JOB_CLEANUP, recording_state_claim_job(&state));
    recording_state_cleaned(&state, true);
    TEST_ASSERT_EQUAL(RECORDING_FAILED, state.phase);
    TEST_ASSERT_EQUAL(RECORDING_EFFECT_NONE, recording_state_record(&state, state.generation));
}

static void test_close_terminal_ends_only_after_forgetting(void)
{
    start_processing();
    recording_state_resolved(&state, ID, RECORDING_REPLY_COMPLETED, false);
    const uint64_t completed = state.generation;
    TEST_ASSERT_EQUAL(RECORDING_EFFECT_VIEW, recording_state_close(&state, completed));
    TEST_ASSERT_EQUAL(RECORDING_EFFECT_NONE, recording_state_close(&state, completed));
    TEST_ASSERT_EQUAL(RECORDING_JOB_FORGET, recording_state_claim_job(&state));
    TEST_ASSERT_EQUAL(RECORDING_EFFECT_NONE, recording_state_close(&state, completed));
    TEST_ASSERT_EQUAL(RECORDING_COMPLETED, state.phase);
    recording_state_forgotten(&state, true, false);
    TEST_ASSERT_EQUAL(RECORDING_FAILURE_RECORD, state.failure);
    TEST_ASSERT_EQUAL_STRING(ID, state.id);

    start_processing();
    recording_state_resolved(&state, ID, RECORDING_REPLY_UNKNOWN, false);
    recording_state_close(&state, state.generation);
    recording_state_claim_job(&state);
    recording_state_forgotten(&state, true, true);
    TEST_ASSERT_EQUAL(RECORDING_IDLE, state.phase);
    TEST_ASSERT_EQUAL_STRING("", state.id);
    TEST_ASSERT_EQUAL(RECORDING_JOB_NONE, recording_state_next_job(&state));
}

static void test_discard_only_an_uncertain_pending_attempt(void)
{
    start_processing();
    TEST_ASSERT_EQUAL(RECORDING_EFFECT_NONE, recording_state_discard(&state, state.generation));
    recording_state_wait_expired(&state, state.generation);
    const uint64_t unconfirmed = state.generation;
    TEST_ASSERT_EQUAL(RECORDING_EFFECT_NONE, recording_state_discard(&state, unconfirmed - 1U));
    TEST_ASSERT_EQUAL(RECORDING_EFFECT_VIEW, recording_state_discard(&state, unconfirmed));
    TEST_ASSERT_EQUAL(RECORDING_EFFECT_NONE, recording_state_discard(&state, unconfirmed));
    TEST_ASSERT_EQUAL(RECORDING_JOB_FORGET, recording_state_claim_job(&state));
    recording_state_forgotten(&state, false, false);
    TEST_ASSERT_EQUAL(RECORDING_UNCONFIRMED, state.phase);
    TEST_ASSERT_EQUAL_STRING(ID, state.id);
    TEST_ASSERT_TRUE(state.cleanup_pending);
    TEST_ASSERT_EQUAL(RECORDING_NOTICE_NOT_CLEARED, view().notice);
    /* The query wanted when the wait ended runs after the forget job. */
    TEST_ASSERT_EQUAL(RECORDING_JOB_QUERY, recording_state_next_job(&state));
    TEST_ASSERT_EQUAL(RECORDING_JOB_NONE, recording_state_next_job(&state));
    TEST_ASSERT_EQUAL(RECORDING_EFFECT_VIEW, recording_state_discard(&state, unconfirmed));
    TEST_ASSERT_EQUAL(RECORDING_NOTICE_NONE, view().notice);

    /* A persisted terminal is no longer pending: only Close ends it. */
    start_processing();
    recording_state_resolved(&state, ID, RECORDING_REPLY_COMPLETED, false);
    TEST_ASSERT_EQUAL(RECORDING_EFFECT_NONE, recording_state_discard(&state, state.generation));
}

static void test_queries_coalesce_while_the_worker_runs(void)
{
    start_processing();
    TEST_ASSERT_EQUAL(RECORDING_EFFECT_VIEW, recording_state_signal(&state, NULL));
    TEST_ASSERT_EQUAL(RECORDING_JOB_QUERY, recording_state_claim_job(&state));
    TEST_ASSERT_EQUAL(RECORDING_EFFECT_NONE, recording_state_signal(&state, OTHER_ID));
    recording_state_signal(&state, ID);
    recording_state_signal(&state, NULL);
    TEST_ASSERT_EQUAL(RECORDING_JOB_NONE, recording_state_claim_job(&state));
    TEST_ASSERT_EQUAL(RECORDING_JOB_QUERY, recording_state_next_job(&state));
    TEST_ASSERT_EQUAL(RECORDING_JOB_NONE, recording_state_next_job(&state));

    recording_state_wait_expired(&state, state.generation);
    recording_state_claim_job(&state);
    TEST_ASSERT_EQUAL(RECORDING_EFFECT_VIEW, recording_state_check(&state, state.generation));
    TEST_ASSERT_EQUAL(RECORDING_EFFECT_NONE, recording_state_check(&state, state.generation));
    TEST_ASSERT_EQUAL(RECORDING_JOB_QUERY, recording_state_next_job(&state));
}

static void test_a_worker_that_cannot_start_touches_nothing(void)
{
    start_recording();
    recording_state_start_failed(&state);
    TEST_ASSERT_EQUAL(RECORDING_FAILURE_START, state.failure);
    TEST_ASSERT_FALSE(state.cleanup_pending);

    start_recording();
    TEST_ASSERT_TRUE(recording_state_captured(&state, RECORDING_MAX_SAMPLES));
    recording_state_ready(&state);
    recording_state_next_job(&state);
    recording_state_send(&state, state.generation);
    recording_state_claim_job(&state);
    recording_state_start_failed(&state);
    TEST_ASSERT_EQUAL(RECORDING_READY_TO_SEND, state.phase);
    TEST_ASSERT_EQUAL(RECORDING_JOB_NONE, recording_state_claim_job(&state));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_boot_cleanup_failure_blocks_capture_until_close_cleans);
    RUN_TEST(test_natural_limit_finalizes_without_sending);
    RUN_TEST(test_short_or_lossy_capture_is_a_local_failure);
    RUN_TEST(test_failed_cleanup_after_capture_failure_blocks_capture);
    RUN_TEST(test_cancel_while_recording_or_ready);
    RUN_TEST(test_double_tap_and_stale_commands_have_no_effect);
    RUN_TEST(test_send_from_ready_without_channel_keeps_the_clip);
    RUN_TEST(test_failed_reservation_blocks_everything);
    RUN_TEST(test_unsent_upload_is_not_sent_only_when_forgotten);
    RUN_TEST(test_ambiguous_upload_is_unconfirmed_until_a_terminal_reply);
    RUN_TEST(test_late_or_foreign_replies_do_not_change_a_terminal);
    RUN_TEST(test_local_wait_marks_unconfirmed_only_while_processing);
    RUN_TEST(test_restore_resumes_pending_and_terminal_attempts);
    RUN_TEST(test_close_terminal_ends_only_after_forgetting);
    RUN_TEST(test_discard_only_an_uncertain_pending_attempt);
    RUN_TEST(test_queries_coalesce_while_the_worker_runs);
    RUN_TEST(test_a_worker_that_cannot_start_touches_nothing);
    return UNITY_END();
}
