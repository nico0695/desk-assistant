#pragma once

#include "board.h"
#include "esp_err.h"

/*
 * Registers the panel and the touch with LVGL. The touch runs in event mode: when on_touch_interrupt fires,
 * the UI task calls ui_display_port_read_touch. Call from the UI task after lv_init().
 */
esp_err_t ui_display_port_init(board_isr_cb_t on_touch_interrupt, void *context);

void ui_display_port_read_touch(void);
