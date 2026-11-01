#pragma once

#include "connectivity_view.h"
#include "lvgl.h"

/* Keeps the latest Wi-Fi view for the UI task. Safe from any task; never blocks. */
void ui_wifi_control_store_view(const connectivity_view_t *view);

/* Applies the latest stored view to the open screens. UI task only. */
void ui_wifi_control_take_view(void);

/* Short state for the Settings button. UI task only. */
connectivity_state_t ui_wifi_control_state(void);

/* Wi-Fi screen on the view applied last; taps post commands to connectivity and never wait. UI task only. */
lv_obj_t *ui_wifi_control_create_screen(lv_event_cb_t on_back);
