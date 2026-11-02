#include "recording_state.h"

static bool current(const recording_state_t *state, uint64_t generation)
{
    return generation == state->generation;
}

static void return_to_idle(recording_state_t *state)
{
    state->phase = RECORDING_IDLE;
    state->failure = RECORDING_FAILURE_NONE;
    state->cleanup_pending = false;
    state->samples = 0;
    ++state->generation;
}

void recording_state_init(recording_state_t *state)
{
    *state = (recording_state_t){
        .phase = RECORDING_CANCELLING,
        .cleanup_pending = true,
        .generation = 1,
    };
}

void recording_state_view(const recording_state_t *state, recording_view_t *view)
{
    *view = (recording_view_t){
        .phase = state->phase,
        .failure = state->failure,
        .duration_ms = (uint32_t)((uint64_t)state->samples * 1000U / RECORDING_SAMPLE_RATE_HZ),
        .generation = state->generation,
    };
}

recording_effect_t recording_state_record(recording_state_t *state, uint64_t generation)
{
    if (!current(state, generation) || state->phase != RECORDING_IDLE || state->cleanup_pending) {
        return RECORDING_EFFECT_NONE;
    }
    state->phase = RECORDING_RECORDING;
    state->samples = 0;
    ++state->generation;
    return RECORDING_EFFECT_START_CAPTURE;
}

recording_effect_t recording_state_send(recording_state_t *state, uint64_t generation)
{
    /* Upload arrives in stage 5b; until then Send only finalizes a running capture. */
    if (!current(state, generation) || state->phase != RECORDING_RECORDING) {
        return RECORDING_EFFECT_NONE;
    }
    state->phase = RECORDING_FINALIZING;
    return RECORDING_EFFECT_NOTIFY_CAPTURE;
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
        state->phase = RECORDING_CANCELLING;
        state->cleanup_pending = true;
        return RECORDING_EFFECT_START_CLEANUP;
    }
    return RECORDING_EFFECT_NONE;
}

recording_effect_t recording_state_close(recording_state_t *state, uint64_t generation)
{
    if (!current(state, generation) || state->phase != RECORDING_FAILED) {
        return RECORDING_EFFECT_NONE;
    }
    if (state->cleanup_pending) {
        state->phase = RECORDING_CANCELLING;
        return RECORDING_EFFECT_START_CLEANUP;
    }
    return_to_idle(state);
    return RECORDING_EFFECT_NONE;
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

void recording_state_ready(recording_state_t *state)
{
    if (state->phase == RECORDING_FINALIZING) {
        state->phase = RECORDING_READY_TO_SEND;
    }
}

void recording_state_failed(recording_state_t *state, recording_failure_t failure, bool cleaned)
{
    state->phase = RECORDING_FAILED;
    state->failure = cleaned ? failure : RECORDING_FAILURE_CLEANUP;
    state->cleanup_pending = !cleaned;
}

void recording_state_cleaned(recording_state_t *state, bool cleaned)
{
    if (state->phase != RECORDING_CANCELLING) {
        return;
    }
    if (cleaned) {
        return_to_idle(state);
    } else {
        recording_state_failed(state, RECORDING_FAILURE_CLEANUP, false);
    }
}
