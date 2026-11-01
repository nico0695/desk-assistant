#pragma once

#include <stdbool.h>

/*
 * Decides which touch samples LVGL may see. While input is disabled (panel off) a press is reported as activity
 * but hidden, and a gesture that starts hidden stays hidden until a valid release. Invalid samples never press.
 */
typedef struct {
    bool enabled;
    bool raw_pressed;
    bool blocked;
    bool activity;
    bool suppress_activity_until_release;
} touch_gate_t;

void touch_gate_init(touch_gate_t *gate);

void touch_gate_set_enabled(touch_gate_t *gate, bool enabled);

/* Returns the pressed state LVGL may see for one sample. */
bool touch_gate_sample(touch_gate_t *gate, bool valid, bool pressed);

/* True while the last valid sample was a press. */
bool touch_gate_held(const touch_gate_t *gate);

/* True when the current gesture must be cancelled in LVGL rather than completed. */
bool touch_gate_blocked(const touch_gate_t *gate);

/* Returns and clears whether a valid press happened since the last call. */
bool touch_gate_take_activity(touch_gate_t *gate);
