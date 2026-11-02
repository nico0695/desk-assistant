#pragma once

#include <stdint.h>

/* Audio contract with the bridge: PCM16 mono WAV, 16 kHz, 500–30000 ms, header synthesized on read. */
#define RECORDING_SAMPLE_RATE_HZ 16000U
#define RECORDING_BYTES_PER_SAMPLE 2U
#define RECORDING_MIN_MS 500U
#define RECORDING_MAX_MS 30000U
#define RECORDING_MIN_SAMPLES (RECORDING_SAMPLE_RATE_HZ * RECORDING_MIN_MS / 1000U)
#define RECORDING_MAX_SAMPLES (RECORDING_SAMPLE_RATE_HZ * RECORDING_MAX_MS / 1000U)
#define RECORDING_MAX_PCM_BYTES (RECORDING_MAX_SAMPLES * RECORDING_BYTES_PER_SAMPLE)
#define RECORDING_WAV_HEADER_BYTES 44U

typedef enum {
    RECORDING_IDLE,
    RECORDING_RECORDING,
    RECORDING_FINALIZING,
    RECORDING_READY_TO_SEND,
    RECORDING_CANCELLING,
    RECORDING_FAILED,
} recording_phase_t;

typedef enum {
    RECORDING_FAILURE_NONE,
    /* The audio hardware or the clip could not start. */
    RECORDING_FAILURE_START,
    RECORDING_FAILURE_TOO_SHORT,
    /* Read error, timeout or lost samples during capture. */
    RECORDING_FAILURE_INTERRUPTED,
    /* The clip could not be written. */
    RECORDING_FAILURE_STORAGE,
    /* Hardware release or clip cleanup failed; capture stays blocked until a cleanup succeeds. */
    RECORDING_FAILURE_CLEANUP,
} recording_failure_t;

/* Snapshot for the view. Commands carry the generation the view showed; any other generation is ignored. */
typedef struct {
    recording_phase_t phase;
    recording_failure_t failure;
    uint32_t duration_ms;
    uint64_t generation;
} recording_view_t;
