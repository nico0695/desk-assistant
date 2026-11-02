#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "recording_view.h"

/* Pure attempt state. The owner applies the returned effect and reports worker outcomes back. */
typedef struct {
    recording_phase_t phase;
    recording_failure_t failure;
    /* The clip or the audio hardware may still hold data or power; blocks a new capture. */
    bool cleanup_pending;
    uint32_t samples;
    uint64_t generation;
} recording_state_t;

typedef enum {
    RECORDING_EFFECT_NONE,
    RECORDING_EFFECT_START_CAPTURE,
    RECORDING_EFFECT_START_CLEANUP,
    /* The running capture worker sees the new phase and acts on it. */
    RECORDING_EFFECT_NOTIFY_CAPTURE,
} recording_effect_t;

/* Starts in CANCELLING with a pending cleanup: boot discards any audio residue before the first capture. */
void recording_state_init(recording_state_t *state);
void recording_state_view(const recording_state_t *state, recording_view_t *view);

recording_effect_t recording_state_record(recording_state_t *state, uint64_t generation);
recording_effect_t recording_state_send(recording_state_t *state, uint64_t generation);
recording_effect_t recording_state_cancel(recording_state_t *state, uint64_t generation);
recording_effect_t recording_state_close(recording_state_t *state, uint64_t generation);

/* Capture progress; returns true when the natural limit is reached and the phase moves to FINALIZING. */
bool recording_state_captured(recording_state_t *state, uint32_t samples);
/* After a clean stop: enough samples and no lost ones make the clip READY_TO_SEND, otherwise a local failure. */
recording_failure_t recording_state_validate(const recording_state_t *state, uint32_t overruns);
void recording_state_ready(recording_state_t *state);
/* A capture failure after the worker tried to release hardware and clear the clip. */
void recording_state_failed(recording_state_t *state, recording_failure_t failure, bool cleaned);
/* Outcome of a cleanup started from CANCELLING. */
void recording_state_cleaned(recording_state_t *state, bool cleaned);
