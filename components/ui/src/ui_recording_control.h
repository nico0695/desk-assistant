#pragma once

#include <stdbool.h>

#include "lvgl.h"
#include "recording.h"

/* Keeps the latest recording view for the UI task. Safe from any task; never blocks. */
void ui_recording_control_store_view(const recording_view_t *view);

/*
 * Applies the latest stored view to the open panel, or to the Home record button when no panel is open; a finished
 * attempt leaves the panel. UI task only.
 */
void ui_recording_control_take_view(void);

/* Label of the Home record button for the latest applied view. UI task only. */
const char *ui_recording_control_entry_text(void);

/* True while capture, finalization or cleanup run: the panel stays on. UI task only. */
bool ui_recording_control_holds_screen(void);

/*
 * Record from Home: starts a capture when the attempt is idle and opens the panel, which shows the current attempt
 * if one exists. Returns NULL when there is nothing to show. on_finished leaves the panel. UI task only.
 */
lv_obj_t *ui_recording_control_open(void (*on_finished)(void));
