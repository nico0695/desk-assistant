#include "ui_modules_control.h"

#include <stddef.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ui_modules.h"

static portMUX_TYPE view_lock = portMUX_INITIALIZER_UNLOCKED;
/* Written by any task under view_lock; applied is the UI task's copy. */
static modules_view_t latest_view;
static modules_view_t applied_view;
/* Set while a module screen is open. */
static void (*finished_handler)(void);

static void leave(void);

static const ui_modules_handlers_t HANDLERS = {
    .on_leave = leave,
    .on_previous = modules_previous,
    .on_next = modules_next,
    .on_refresh = modules_refresh,
    .on_open = modules_open_detail,
    .on_back = modules_back,
    .read_page = modules_read_page,
    .read_detail = modules_read_detail,
};

void ui_modules_control_store_view(const modules_view_t *view)
{
    taskENTER_CRITICAL(&view_lock);
    /* Views are published outside the owner's lock; an older one may arrive after a newer one. */
    if (view->revision >= latest_view.revision) latest_view = *view;
    taskEXIT_CRITICAL(&view_lock);
}

static void apply_latest(void)
{
    taskENTER_CRITICAL(&view_lock);
    applied_view = latest_view;
    taskEXIT_CRITICAL(&view_lock);
}

static void leave(void)
{
    if (finished_handler != NULL) finished_handler();
}

/* Whatever replaces the screen, the module stops holding its lists and details. */
static void forget_screen(lv_event_t *event)
{
    (void)event;
    finished_handler = NULL;
    modules_leave();
}

void ui_modules_control_take_view(void)
{
    if (finished_handler == NULL) return;
    apply_latest();
    ui_modules_show(&applied_view);
}

lv_obj_t *ui_modules_control_open(bridge_resource_kind_t kind, void (*on_finished)(void))
{
    modules_enter(kind);
    /* The command publishes synchronously; show its first view. */
    apply_latest();
    finished_handler = on_finished;
    lv_obj_t *screen = ui_modules_screen_create(&applied_view, &HANDLERS);
    lv_obj_add_event_cb(screen, forget_screen, LV_EVENT_DELETE, NULL);
    return screen;
}
