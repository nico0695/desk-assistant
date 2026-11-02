#include "recording_state.h"

#include <string.h>

static bool current(const recording_state_t *state, uint64_t generation)
{
    return generation == state->generation;
}

static bool has_attempt(const recording_state_t *state)
{
    return state->id[0] != '\0';
}

static bool same_attempt(const recording_state_t *state, const char *id)
{
    return has_attempt(state) && id != NULL && strcmp(state->id, id) == 0;
}

static bool pending(const recording_state_t *state)
{
    return state->phase == RECORDING_UPLOADING || state->phase == RECORDING_PROCESSING ||
           state->phase == RECORDING_UNCONFIRMED;
}

static bool terminal(const recording_state_t *state)
{
    return state->phase == RECORDING_COMPLETED || state->phase == RECORDING_UNKNOWN ||
           (state->phase == RECORDING_FAILED && state->failure == RECORDING_FAILURE_REMOTE);
}

static bool record_blocked(const recording_state_t *state)
{
    return state->phase == RECORDING_FAILED && state->failure == RECORDING_FAILURE_RECORD;
}

static bool can_check(const recording_state_t *state)
{
    return has_attempt(state) &&
           (state->phase == RECORDING_UNCONFIRMED || (terminal(state) && !state->has_message));
}

static bool forgetting(const recording_state_t *state)
{
    return state->wanted_job == RECORDING_JOB_FORGET || state->running_job == RECORDING_JOB_FORGET;
}

/* Every attempt phase change gets a new generation, so commands from an older view are ignored. */
static void enter(recording_state_t *state, recording_phase_t phase, recording_failure_t failure)
{
    state->phase = phase;
    state->failure = failure;
    ++state->generation;
}

static void forget_attempt(recording_state_t *state)
{
    memset(state->id, 0, sizeof(state->id));
    state->durable_pending = false;
    state->has_message = false;
    state->content_omitted = false;
    state->query_wanted = false;
}

static void block_record(recording_state_t *state)
{
    state->record_failed = true;
    state->query_wanted = false;
    enter(state, RECORDING_FAILED, RECORDING_FAILURE_RECORD);
}

static void return_to_idle(recording_state_t *state)
{
    forget_attempt(state);
    state->notice = RECORDING_NOTICE_NONE;
    state->cleanup_pending = false;
    state->send_requested = false;
    state->samples = 0;
    enter(state, RECORDING_IDLE, RECORDING_FAILURE_NONE);
}

static recording_effect_t want_job(recording_state_t *state, recording_job_t job)
{
    state->wanted_job = job;
    return RECORDING_EFFECT_VIEW;
}

static recording_phase_t terminal_phase(recording_reply_t reply)
{
    return reply == RECORDING_REPLY_COMPLETED ? RECORDING_COMPLETED
           : reply == RECORDING_REPLY_FAILED  ? RECORDING_FAILED
                                              : RECORDING_UNKNOWN;
}

static recording_failure_t terminal_failure(recording_reply_t reply)
{
    return reply == RECORDING_REPLY_FAILED ? RECORDING_FAILURE_REMOTE : RECORDING_FAILURE_NONE;
}

/* The shown terminal matches reply and still lacks its text, as after a restart. */
static bool missing_text_of(const recording_state_t *state, recording_reply_t reply)
{
    return terminal(state) && !state->has_message && state->phase == terminal_phase(reply);
}

void recording_state_init(recording_state_t *state)
{
    *state = (recording_state_t){
        .phase = RECORDING_CANCELLING,
        .cleanup_pending = true,
        .generation = 1,
        .wanted_job = RECORDING_JOB_CLEANUP,
    };
}

void recording_state_restore(recording_state_t *state, const char *id, recording_reply_t stored)
{
    if (!bridge_uuid_valid(id)) {
        return;
    }
    memcpy(state->id, id, sizeof(state->id));
    state->query_wanted = true;
    state->durable_pending = stored == RECORDING_REPLY_PROCESSING;
    if (stored == RECORDING_REPLY_PROCESSING) {
        enter(state, RECORDING_PROCESSING, RECORDING_FAILURE_NONE);
    } else {
        enter(state, terminal_phase(stored), terminal_failure(stored));
    }
}

void recording_state_restore_failed(recording_state_t *state)
{
    block_record(state);
}

void recording_state_view(const recording_state_t *state, recording_view_t *view)
{
    *view = (recording_view_t){
        .phase = state->phase,
        .failure = state->failure,
        .notice = state->notice,
        .duration_ms = (uint32_t)((uint64_t)state->samples * 1000U / RECORDING_SAMPLE_RATE_HZ),
        .generation = state->generation,
        .has_message = state->has_message,
        .content_omitted = state->content_omitted,
        .can_check = can_check(state),
        .can_discard = state->phase == RECORDING_UNCONFIRMED && state->durable_pending && !state->record_failed,
        .close_keeps_attempt = pending(state) || record_blocked(state),
    };
}

recording_job_t recording_state_claim_job(recording_state_t *state)
{
    if (state->running_job != RECORDING_JOB_NONE) {
        return RECORDING_JOB_NONE;
    }
    if (state->wanted_job != RECORDING_JOB_NONE) {
        state->running_job = state->wanted_job;
        state->wanted_job = RECORDING_JOB_NONE;
    } else if (state->query_wanted) {
        state->query_wanted = false;
        if (!record_blocked(state) && has_attempt(state) &&
            (state->phase == RECORDING_PROCESSING || can_check(state))) {
            state->running_job = RECORDING_JOB_QUERY;
        }
    }
    return state->running_job;
}

recording_job_t recording_state_next_job(recording_state_t *state)
{
    state->running_job = RECORDING_JOB_NONE;
    return recording_state_claim_job(state);
}

void recording_state_start_failed(recording_state_t *state)
{
    const recording_job_t job = state->running_job;
    state->running_job = RECORDING_JOB_NONE;
    if (job == RECORDING_JOB_CAPTURE) {
        recording_state_failed(state, RECORDING_FAILURE_START, true);
    } else if (job == RECORDING_JOB_CLEANUP) {
        recording_state_failed(state, RECORDING_FAILURE_CLEANUP, false);
    } else if (job == RECORDING_JOB_UPLOAD && state->phase == RECORDING_UPLOADING && !has_attempt(state)) {
        state->phase = RECORDING_READY_TO_SEND;
    }
}

recording_effect_t recording_state_record(recording_state_t *state, uint64_t generation)
{
    if (!current(state, generation) || state->phase != RECORDING_IDLE || state->cleanup_pending) {
        return RECORDING_EFFECT_NONE;
    }
    state->samples = 0;
    enter(state, RECORDING_RECORDING, RECORDING_FAILURE_NONE);
    return want_job(state, RECORDING_JOB_CAPTURE);
}

recording_effect_t recording_state_send(recording_state_t *state, uint64_t generation)
{
    if (!current(state, generation)) {
        return RECORDING_EFFECT_NONE;
    }
    if (state->phase == RECORDING_RECORDING) {
        state->phase = RECORDING_FINALIZING;
        state->send_requested = true;
        return RECORDING_EFFECT_NOTIFY_CAPTURE;
    }
    if (state->phase == RECORDING_READY_TO_SEND) {
        state->notice = RECORDING_NOTICE_NONE;
        state->phase = RECORDING_UPLOADING;
        return want_job(state, RECORDING_JOB_UPLOAD);
    }
    return RECORDING_EFFECT_NONE;
}

recording_effect_t recording_state_cancel(recording_state_t *state, uint64_t generation)
{
    if (!current(state, generation)) {
        return RECORDING_EFFECT_NONE;
    }
    if (state->phase == RECORDING_RECORDING) {
        state->phase = RECORDING_CANCELLING;
        return RECORDING_EFFECT_NOTIFY_CAPTURE;
    }
    if (state->phase == RECORDING_READY_TO_SEND) {
        state->notice = RECORDING_NOTICE_NONE;
        state->phase = RECORDING_CANCELLING;
        state->cleanup_pending = true;
        return want_job(state, RECORDING_JOB_CLEANUP);
    }
    return RECORDING_EFFECT_NONE;
}

recording_effect_t recording_state_close(recording_state_t *state, uint64_t generation)
{
    if (!current(state, generation) || record_blocked(state)) {
        return RECORDING_EFFECT_NONE;
    }
    if (terminal(state)) {
        return forgetting(state) ? RECORDING_EFFECT_NONE : want_job(state, RECORDING_JOB_FORGET);
    }
    if (state->phase != RECORDING_FAILED) {
        return RECORDING_EFFECT_NONE;
    }
    if (state->cleanup_pending) {
        state->phase = RECORDING_CANCELLING;
        return want_job(state, RECORDING_JOB_CLEANUP);
    }
    return_to_idle(state);
    return RECORDING_EFFECT_VIEW;
}

recording_effect_t recording_state_check(recording_state_t *state, uint64_t generation)
{
    if (!current(state, generation) || !can_check(state) || state->query_wanted) {
        return RECORDING_EFFECT_NONE;
    }
    state->query_wanted = true;
    return RECORDING_EFFECT_VIEW;
}

recording_effect_t recording_state_discard(recording_state_t *state, uint64_t generation)
{
    recording_view_t view;
    recording_state_view(state, &view);
    if (!current(state, generation) || !view.can_discard || forgetting(state)) {
        return RECORDING_EFFECT_NONE;
    }
    return want_job(state, RECORDING_JOB_FORGET);
}

bool recording_state_captured(recording_state_t *state, uint32_t samples)
{
    state->samples = samples;
    if (state->phase == RECORDING_RECORDING && samples >= RECORDING_MAX_SAMPLES) {
        state->phase = RECORDING_FINALIZING;
        return true;
    }
    return false;
}

recording_failure_t recording_state_validate(const recording_state_t *state, uint32_t overruns)
{
    if (overruns != 0U) {
        return RECORDING_FAILURE_INTERRUPTED;
    }
    return state->samples < RECORDING_MIN_SAMPLES ? RECORDING_FAILURE_TOO_SHORT : RECORDING_FAILURE_NONE;
}

bool recording_state_ready(recording_state_t *state)
{
    if (state->phase != RECORDING_FINALIZING) {
        return false;
    }
    if (!state->send_requested) {
        state->phase = RECORDING_READY_TO_SEND;
        return false;
    }
    state->send_requested = false;
    state->phase = RECORDING_UPLOADING;
    state->running_job = RECORDING_JOB_UPLOAD;
    return true;
}

void recording_state_failed(recording_state_t *state, recording_failure_t failure, bool cleaned)
{
    state->phase = RECORDING_FAILED;
    state->failure = cleaned ? failure : RECORDING_FAILURE_CLEANUP;
    state->cleanup_pending = !cleaned;
    state->send_requested = false;
    state->wanted_job = RECORDING_JOB_NONE;
}

void recording_state_cleaned(recording_state_t *state, bool cleaned)
{
    if (state->phase == RECORDING_CANCELLING) {
        if (cleaned) {
            return_to_idle(state);
        } else {
            recording_state_failed(state, RECORDING_FAILURE_CLEANUP, false);
        }
    } else if (state->phase != RECORDING_IDLE) {
        state->cleanup_pending = !cleaned;
    }
}

void recording_state_refused(recording_state_t *state)
{
    if (state->phase == RECORDING_UPLOADING && !has_attempt(state)) {
        state->phase = RECORDING_READY_TO_SEND;
        state->notice = RECORDING_NOTICE_NOT_CONNECTED;
    }
}

void recording_state_reserved(recording_state_t *state, const char *id, bool stored)
{
    if (state->phase != RECORDING_UPLOADING || has_attempt(state) || !bridge_uuid_valid(id)) {
        return;
    }
    memcpy(state->id, id, sizeof(state->id));
    if (stored) {
        state->durable_pending = true;
    } else {
        block_record(state);
    }
}

void recording_state_upload_failed(recording_state_t *state, const char *id, recording_upload_failure_t failure)
{
    if (state->phase != RECORDING_UPLOADING || !same_attempt(state, id)) {
        return;
    }
    if (failure == RECORDING_UPLOAD_UNSENT) {
        forget_attempt(state);
        /* "Not sent" also tells the audio was deleted; a pending cleanup reports that instead. */
        enter(state, RECORDING_FAILED,
              state->cleanup_pending ? RECORDING_FAILURE_CLEANUP : RECORDING_FAILURE_NOT_SENT);
        return;
    }
    if (failure == RECORDING_UPLOAD_UNSENT_KEPT) {
        state->record_failed = true;
    }
    state->query_wanted = true;
    enter(state, RECORDING_UNCONFIRMED, RECORDING_FAILURE_NONE);
}

recording_apply_t recording_state_reply(recording_state_t *state, const char *id, recording_reply_t reply)
{
    if (!same_attempt(state, id) || record_blocked(state)) {
        return RECORDING_APPLY_IGNORE;
    }
    if (reply == RECORDING_REPLY_PROCESSING) {
        /* Processing never clears local uncertainty: only a terminal reply does. */
        if (state->phase != RECORDING_UPLOADING) {
            return RECORDING_APPLY_IGNORE;
        }
        state->query_wanted = true;
        enter(state, RECORDING_PROCESSING, RECORDING_FAILURE_NONE);
        return RECORDING_APPLY_WAIT;
    }
    if (pending(state)) {
        return RECORDING_APPLY_STORE;
    }
    return missing_text_of(state, reply) ? RECORDING_APPLY_SHOW : RECORDING_APPLY_IGNORE;
}

void recording_state_resolved(recording_state_t *state, const char *id, recording_reply_t reply, bool omitted)
{
    if (reply == RECORDING_REPLY_PROCESSING || !same_attempt(state, id) || record_blocked(state) ||
        (!pending(state) && !missing_text_of(state, reply))) {
        return;
    }
    state->durable_pending = false;
    state->has_message = true;
    state->content_omitted = omitted;
    state->query_wanted = false;
    enter(state, terminal_phase(reply), terminal_failure(reply));
}

void recording_state_store_failed(recording_state_t *state, const char *id)
{
    if (same_attempt(state, id)) {
        block_record(state);
    }
}

recording_effect_t recording_state_wait_expired(recording_state_t *state, uint64_t generation)
{
    if (!current(state, generation) || state->phase != RECORDING_PROCESSING) {
        return RECORDING_EFFECT_NONE;
    }
    state->query_wanted = true;
    enter(state, RECORDING_UNCONFIRMED, RECORDING_FAILURE_NONE);
    return RECORDING_EFFECT_VIEW;
}

recording_effect_t recording_state_signal(recording_state_t *state, const char *id)
{
    if (!has_attempt(state) || (id != NULL && !same_attempt(state, id))) {
        return RECORDING_EFFECT_NONE;
    }
    state->query_wanted = true;
    return RECORDING_EFFECT_VIEW;
}

void recording_state_forgotten(recording_state_t *state, bool cleaned, bool cleared)
{
    if (!has_attempt(state)) {
        return;
    }
    state->cleanup_pending = !cleaned;
    if (!cleared) {
        block_record(state);
        return;
    }
    if (cleaned) {
        return_to_idle(state);
        return;
    }
    forget_attempt(state);
    enter(state, RECORDING_FAILED, RECORDING_FAILURE_CLEANUP);
}
