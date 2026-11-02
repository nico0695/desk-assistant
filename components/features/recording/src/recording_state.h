#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "bridge_uuid.h"
#include "recording_view.h"

/* Work for the single on-demand worker; at most one runs at a time. */
typedef enum {
    RECORDING_JOB_NONE,
    RECORDING_JOB_CAPTURE,
    RECORDING_JOB_CLEANUP,
    /* Admission, reservation, the single PUT and the clip cleanup. */
    RECORDING_JOB_UPLOAD,
    /* One GET of the attempt. */
    RECORDING_JOB_QUERY,
    /* Clears the clip and then the durable record of the attempt. */
    RECORDING_JOB_FORGET,
} recording_job_t;

/* Pure attempt state. The owner applies the returned effect and reports worker outcomes back. */
typedef struct {
    recording_phase_t phase;
    recording_failure_t failure;
    recording_notice_t notice;
    /* The clip or the audio hardware may still hold data or power; blocks a new capture. */
    bool cleanup_pending;
    uint32_t samples;
    uint64_t generation;
    /* Send was tapped while recording: a valid clip continues to the upload without another tap. */
    bool send_requested;
    /* Empty without an attempt. */
    char id[BRIDGE_UUID_SIZE];
    /* The durable record holds this attempt as pending. */
    bool durable_pending;
    /* A record write failed and may have reached flash; the record cannot be trusted until restart. */
    bool record_failed;
    bool has_message;
    bool content_omitted;
    recording_job_t running_job;
    recording_job_t wanted_job;
    bool query_wanted;
} recording_state_t;

typedef enum {
    RECORDING_EFFECT_NONE,
    /* The state changed: publish it and claim any job. */
    RECORDING_EFFECT_VIEW,
    /* The running capture worker sees the new phase and acts on it. */
    RECORDING_EFFECT_NOTIFY_CAPTURE,
} recording_effect_t;

/* Status of a validated reply for this attempt. */
typedef enum {
    RECORDING_REPLY_PROCESSING,
    RECORDING_REPLY_COMPLETED,
    RECORDING_REPLY_FAILED,
    RECORDING_REPLY_UNKNOWN,
} recording_reply_t;

typedef enum {
    RECORDING_APPLY_IGNORE,
    /* Applied; arm the local wait. */
    RECORDING_APPLY_WAIT,
    /* Persist the terminal phase, then call recording_state_resolved. */
    RECORDING_APPLY_STORE,
    /* Already persisted; call recording_state_resolved to show the text. */
    RECORDING_APPLY_SHOW,
} recording_apply_t;

typedef enum {
    /* Failed before the last block and the reservation was cleared: nothing reached the bridge. */
    RECORDING_UPLOAD_UNSENT,
    /* As above, but clearing the reservation failed. */
    RECORDING_UPLOAD_UNSENT_KEPT,
    /* Any other failure: the bridge may have the attempt. */
    RECORDING_UPLOAD_AMBIGUOUS,
} recording_upload_failure_t;

/* Starts in CANCELLING with a pending cleanup: boot discards any audio residue before the first capture. */
void recording_state_init(recording_state_t *state);
/*
 * After init: a stored attempt resumes and is queried. A pending one (RECORDING_REPLY_PROCESSING) arms the local wait;
 * a terminal one is shown without its text.
 */
void recording_state_restore(recording_state_t *state, const char *id, recording_reply_t stored);
/* After init: an unreadable record blocks everything until restart. */
void recording_state_restore_failed(recording_state_t *state);
void recording_state_view(const recording_state_t *state, recording_view_t *view);

/* The next job, marking the worker busy; RECORDING_JOB_NONE while one runs or when nothing is wanted. */
recording_job_t recording_state_claim_job(recording_state_t *state);
/* The worker finished its job; returns the next one it must run, or RECORDING_JOB_NONE after going idle. */
recording_job_t recording_state_next_job(recording_state_t *state);
/* The claimed job could not start a worker: nothing it would touch was touched. */
void recording_state_start_failed(recording_state_t *state);

recording_effect_t recording_state_record(recording_state_t *state, uint64_t generation);
recording_effect_t recording_state_send(recording_state_t *state, uint64_t generation);
recording_effect_t recording_state_cancel(recording_state_t *state, uint64_t generation);
recording_effect_t recording_state_close(recording_state_t *state, uint64_t generation);
recording_effect_t recording_state_check(recording_state_t *state, uint64_t generation);
recording_effect_t recording_state_discard(recording_state_t *state, uint64_t generation);

/* Capture progress; returns true when the natural limit is reached and the phase moves to FINALIZING. */
bool recording_state_captured(recording_state_t *state, uint32_t samples);
/* After a clean stop: enough samples and no lost ones make the clip READY_TO_SEND, otherwise a local failure. */
recording_failure_t recording_state_validate(const recording_state_t *state, uint32_t overruns);
/* The clip is valid; returns true when Send was already tapped and the job continues with the upload. */
bool recording_state_ready(recording_state_t *state);
/* A capture failure after the worker tried to release hardware and clear the clip; drops any wanted job. */
void recording_state_failed(recording_state_t *state, recording_failure_t failure, bool cleaned);
/* Outcome of a clip cleanup: ends CANCELLING, otherwise only updates the pending cleanup of an active phase. */
void recording_state_cleaned(recording_state_t *state, bool cleaned);

/* Upload admission refused: the event stream is closed or the origin cannot be held. The clip is kept. */
void recording_state_refused(recording_state_t *state);
/* The identity was written durably, or the write failed and may have reached flash. */
void recording_state_reserved(recording_state_t *state, const char *id, bool stored);
/* An uncertain failure (not RECORDING_UPLOAD_UNSENT) arms the local wait, like a processing reply. */
void recording_state_upload_failed(recording_state_t *state, const char *id, recording_upload_failure_t failure);
/* A validated reply from the PUT or a GET. */
recording_apply_t recording_state_reply(recording_state_t *state, const char *id, recording_reply_t reply);
/* A terminal reply after RECORDING_APPLY_STORE succeeded, or after RECORDING_APPLY_SHOW. */
void recording_state_resolved(recording_state_t *state, const char *id, recording_reply_t reply, bool omitted);
/* Persisting a terminal phase failed. */
void recording_state_store_failed(recording_state_t *state, const char *id);
/* The local wait of the attempt shown with generation ended: a pending attempt becomes UNCONFIRMED and is queried. */
recording_effect_t recording_state_wait_expired(recording_state_t *state, uint64_t generation);
/* The event stream opened, or signaled a result for id (NULL for any attempt). */
recording_effect_t recording_state_signal(recording_state_t *state, const char *id);
/* Outcome of RECORDING_JOB_FORGET: the clip was cleaned and only then the record cleared. */
void recording_state_forgotten(recording_state_t *state, bool cleaned, bool cleared);
