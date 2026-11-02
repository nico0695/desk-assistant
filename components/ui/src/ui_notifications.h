#pragma once

#include "lvgl.h"
#include "notification_view.h"

typedef struct {
    void (*close)(uint64_t generation, const char *event_id);
    void (*snooze)(uint64_t generation, const char *event_id, unsigned preset);
} ui_notifications_handlers_t;

/* UI owner only. The card is on LVGL's top layer, preserving the current screen and scroll beneath it. */
void ui_notifications_show(const notification_view_t *view, const ui_notifications_handlers_t *handlers);
void ui_notifications_hide(void);
