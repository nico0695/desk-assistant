#include "ui_recording_control.h"

#include <stddef.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ui_recording.h"

static portMUX_TYPE view_lock = portMUX_INITIALIZER_UNLOCKED;
/* Written by any task under view_lock; applied is the UI task's copy. */
static recording_view_t latest_view;
static recording_view_t applied_view;
/* Set while the panel is open. */
static void (*finished_handler)(void);

static const ui_recording_handlers_t HANDLERS = {
    .on_send = recording_send,
    .on_cancel = recording_cancel,
    .on_close = recording_close,
};

void ui_recording_control_store_view(const recording_view_t *view)
{
    taskENTER_CRITICAL(&view_lock);
    latest_view = *view;
    taskEXIT_CRITICAL(&view_lock);
}

static void apply_latest(void)
{
    taskENTER_CRITICAL(&view_lock);
    applied_view = latest_view;
    taskEXIT_CRITICAL(&view_lock);
}

static void forget_panel(lv_event_t *event)
{
    (void)event;
    finished_handler = NULL;
}

void ui_recording_control_take_view(void)
{
    apply_latest();
    if (finished_handler == NULL) {
        return;
    }
    if (applied_view.phase == RECORDING_IDLE) {
        finished_handler();
        return;
    }
    ui_recording_show(&applied_view);
}

bool ui_recording_control_holds_screen(void)
{
    return applied_view.phase == RECORDING_RECORDING || applied_view.phase == RECORDING_FINALIZING ||
           applied_view.phase == RECORDING_CANCELLING;
}

lv_obj_t *ui_recording_control_open(void (*on_finished)(void))
{
    apply_latest();
    if (applied_view.phase == RECORDING_IDLE) {
        recording_record(applied_view.generation);
        /* The command publishes synchronously; show its result. */
        apply_latest();
    }
    if (applied_view.phase == RECORDING_IDLE) {
        return NULL;
    }
    finished_handler = on_finished;
    lv_obj_t *screen = ui_recording_screen_create(&applied_view, &HANDLERS);
    lv_obj_add_event_cb(screen, forget_panel, LV_EVENT_DELETE, NULL);
    return screen;
}
