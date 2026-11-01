#pragma once

#include <stdint.h>

#include "lvgl.h"
#include "settings_values.h"

typedef struct {
    /* Applies a new screen timeout to the screen state. */
    void (*apply_timeout)(uint8_t seconds);
    /* Wakes the UI task from another task; it then calls ui_settings_control_take_saved. */
    void (*notify_saved)(void);
} ui_settings_control_hooks_t;

/* Owns the Settings rows for the UI task. Call once from the UI task with the settings loaded at boot. */
void ui_settings_control_init(const settings_loaded_t *loaded, const ui_settings_control_hooks_t *hooks);

lv_obj_t *ui_settings_control_create_screen(lv_event_cb_t on_back);

/* Applies the save results received since the last call. UI task only. */
void ui_settings_control_take_saved(void);

/* The panel went off mid-gesture: a drag in progress ends as a release. UI task only. */
void ui_settings_control_end_gesture(void);
