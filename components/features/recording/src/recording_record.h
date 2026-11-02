#pragma once

#include "bridge_uuid.h"
#include "esp_err.h"

/* Durable phase of the attempt, independent of the view. Never holds audio or reply text. */
typedef enum {
    RECORDING_RECORD_NONE,
    /* Reserved before the upload; the remote result is not known. */
    RECORDING_RECORD_PENDING,
    RECORDING_RECORD_COMPLETED,
    RECORDING_RECORD_FAILED,
    RECORDING_RECORD_UNKNOWN,
} recording_record_phase_t;

typedef struct {
    recording_record_phase_t phase;
    /* Empty when phase is RECORDING_RECORD_NONE. */
    char id[BRIDGE_UUID_SIZE];
} recording_record_t;

/*
 * ESP_OK with the stored attempt, or with RECORDING_RECORD_NONE when none was ever stored. Any other error (unreadable,
 * incompatible or invalid record) must block new attempts: it is never read as empty and nothing is erased.
 */
esp_err_t recording_record_load(recording_record_t *record);

/*
 * Each write replaces the record with one commit. A failed write may still have reached flash, so the caller must
 * not assume the previous record survived or that the new one is absent.
 */
esp_err_t recording_record_begin(const char *id);
esp_err_t recording_record_finish(const char *id, recording_record_phase_t terminal);
esp_err_t recording_record_clear(void);
