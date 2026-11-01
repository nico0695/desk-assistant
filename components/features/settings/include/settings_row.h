#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "settings_values.h"

/*
 * State of one Settings row, kept by the UI owner while the screen comes and goes. Transitions return the
 * effects the caller runs: apply the value to the device and report the result, and save it with the sequence.
 */
typedef struct {
    settings_id_t id;
    uint8_t confirmed;
    uint8_t shown;
    uint8_t applied;
    bool dragging;
    bool load_failed;
    bool apply_failed;
    bool save_failed;
    uint32_t save_sequence;
} settings_row_t;

typedef struct {
    bool apply;
    bool save;
    uint8_t value;
    uint32_t save_sequence;
} settings_row_effect_t;

typedef enum {
    SETTINGS_ROW_ERROR_NONE,
    SETTINGS_ROW_ERROR_LOAD,
    SETTINGS_ROW_ERROR_APPLY,
    SETTINGS_ROW_ERROR_SAVE,
} settings_row_error_t;

void settings_row_init(settings_row_t *row, settings_id_t id, uint8_t value, bool load_failed);

/* The value follows the finger. Brightness asks to apply each change; other settings only show it. */
settings_row_effect_t settings_row_drag(settings_row_t *row, uint8_t value);

/* Release or lost gesture: confirms and saves the last applied (brightness) or shown value if it changed. */
settings_row_effect_t settings_row_release(settings_row_t *row);

/* Leaving or rebuilding mid-drag: shows the confirmed value again and reapplies it if needed. */
settings_row_effect_t settings_row_cancel(settings_row_t *row);

/* A single choice (screen timeout): confirms, applies and saves at once. */
settings_row_effect_t settings_row_select(settings_row_t *row, uint8_t value);

/*
 * A value saved outside Settings (USB provisioning): confirmed, shown and saved, with earlier errors cleared and any
 * save still in flight made stale. Asks to apply it.
 */
settings_row_effect_t settings_row_external(settings_row_t *row, uint8_t value);

/* A failed apply is not counted as applied. */
void settings_row_apply_result(settings_row_t *row, uint8_t value, bool ok);

/* Only the result of the latest save request changes the row. */
void settings_row_save_result(settings_row_t *row, uint32_t save_sequence, bool ok);

/* The error to show: a current apply or save failure before a load failure. */
settings_row_error_t settings_row_error(const settings_row_t *row);
