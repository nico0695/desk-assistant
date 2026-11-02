#pragma once

#include "lvgl.h"
#include "modules.h"

/* Keeps the newest module view for the UI task. Safe from any task; never blocks. */
void ui_modules_control_store_view(const modules_view_t *view);

/* Applies the newest stored view to the open module screen; does nothing when none is open. UI task only. */
void ui_modules_control_take_view(void);

/*
 * Opens a module: starts its first read and returns the screen showing it. Leaving through Back calls on_finished,
 * and deleting the screen by any path frees what the module holds. UI task only.
 */
lv_obj_t *ui_modules_control_open(bridge_resource_kind_t kind, void (*on_finished)(void));
