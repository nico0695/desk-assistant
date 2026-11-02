#pragma once

#include <stdbool.h>
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
    /* Admission, reservation of the durable identity and the single upload. */
    RECORDING_UPLOADING,
    /* The bridge acknowledged the attempt; the result is pending. */
    RECORDING_PROCESSING,
    RECORDING_COMPLETED,
    /* A local failure, or a known remote failure when failure is RECORDING_FAILURE_REMOTE. */
    RECORDING_FAILED,
    /* The remote result is terminal but unknown. */
    RECORDING_UNKNOWN,
    /* Local uncertainty about the attempt; not a remote status. */
    RECORDING_UNCONFIRMED,
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
    /* The upload failed before its last block and the reservation was forgotten: nothing reached the bridge. */
    RECORDING_FAILURE_NOT_SENT,
    /* The bridge reported the attempt as failed; its message is available. */
    RECORDING_FAILURE_REMOTE,
    /*
     * The attempt record could not be read or written safely. As FAILED everything stays blocked until restart; as
     * UNCONFIRMED (the reservation could not be cleared) the attempt can still be checked but not discarded.
     */
    RECORDING_FAILURE_RECORD,
} recording_failure_t;

typedef enum {
    RECORDING_NOTICE_NONE,
    /* Send was refused locally because the event stream is closed; the clip is kept. */
    RECORDING_NOTICE_NOT_CONNECTED,
    /* Close or Discard could not clear the audio; the attempt is kept and the action can be repeated. */
    RECORDING_NOTICE_NOT_CLEARED,
} recording_notice_t;

/*
 * Snapshot for the view. Commands carry the generation the view showed; any other generation is ignored. revision
 * grows with every published view, so a consumer can drop one older than the view it already has.
 */
typedef struct {
    recording_phase_t phase;
    recording_failure_t failure;
    recording_notice_t notice;
    uint32_t duration_ms;
    uint64_t generation;
    uint64_t revision;
    /* The bridge message of a terminal result can be read with recording_read_message. */
    bool has_message;
    bool content_omitted;
    /* Check reads the attempt again; it never resends audio. */
    bool can_check;
    /* Discard is allowed: uncertain attempt with a pending durable phase and trustworthy storage. */
    bool can_discard;
    /* Close only leaves the panel: the attempt is kept. Otherwise Close ends the attempt. */
    bool close_keeps_attempt;
} recording_view_t;
