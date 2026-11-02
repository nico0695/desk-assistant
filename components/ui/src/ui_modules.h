#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"
#include "bridge_snooze.h"
#include "modules_view.h"

/* Each command carries the ticket of the view the user saw. */
typedef struct {
    /* Back from a list leaves the module. */
    void (*on_leave)(void);
    void (*on_previous)(uint64_t ticket);
    void (*on_next)(uint64_t ticket);
    void (*on_refresh)(uint64_t ticket);
    void (*on_open)(uint64_t ticket, uint64_t id);
    /* Back from a detail returns to its list. */
    void (*on_back)(uint64_t ticket);
    /* Actions on the detail shown with the ticket: Delete and Snooze after the screen's dialog, Complete on its tap. */
    void (*on_delete)(uint64_t ticket);
    void (*on_complete)(uint64_t ticket);
    void (*on_snooze)(uint64_t ticket, bridge_snooze_preset_t preset);
    /* The readers copy what they need before returning and never call back into the owner. */
    bool (*read_page)(uint64_t ticket, void (*reader)(const bridge_resource_page_t *page, void *context),
                      void *context);
    bool (*read_detail)(uint64_t ticket, void (*reader)(const bridge_resource_detail_t *detail, void *context),
                        void *context);
} ui_modules_handlers_t;

/*
 * List or detail of a module with the controls of the view: loading, empty and failed lists replace the rows, and a
 * detail scrolls while Back stays fixed. UI task only.
 */
lv_obj_t *ui_modules_screen_create(const modules_view_t *view, const ui_modules_handlers_t *handlers);

/* Rebuilds the open screen for this view; does nothing when it is closed. UI task only. */
void ui_modules_show(const modules_view_t *view);

/* True while a Delete or Snooze dialog waits for the user. UI task only. */
bool ui_modules_dialog_open(void);
