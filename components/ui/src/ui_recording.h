#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"
#include "recording_view.h"

/* Each command carries the generation of the view the user saw. */
typedef struct {
    void (*on_send)(uint64_t generation);
    void (*on_cancel)(uint64_t generation);
    void (*on_close)(uint64_t generation);
    void (*on_check)(uint64_t generation);
    /* Called only after the user confirmed on the panel. */
    void (*on_discard)(uint64_t generation);
    bool (*read_message)(uint64_t generation, void (*reader)(const char *text, void *context), void *context);
} ui_recording_handlers_t;

/*
 * Recording panel: state title, duration or status message, the bridge reply with its notes, and the actions of the
 * current phase. Discard asks for confirmation on the panel first. UI task only.
 */
lv_obj_t *ui_recording_screen_create(const recording_view_t *view, const ui_recording_handlers_t *handlers);

/* Updates the open panel; does nothing when it is closed. UI task only. */
void ui_recording_show(const recording_view_t *view);
