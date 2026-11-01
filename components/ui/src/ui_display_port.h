#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "board.h"
#include "esp_err.h"

/*
 * Registers the panel and the touch with LVGL. The touch runs in event mode: when on_touch_interrupt fires,
 * the UI task calls ui_display_port_read_touch. Applies the initial brightness. Call from the UI task after lv_init().
 */
esp_err_t ui_display_port_init(board_isr_cb_t on_touch_interrupt, void *context, uint8_t brightness_percent);

void ui_display_port_read_touch(void);

/* True while a press hidden from LVGL is still down. LVGL does not poll hidden presses, so the caller reads them. */
bool ui_display_port_hidden_press_held(void);

/* True when a valid press happened since the last call, including presses hidden from LVGL. */
bool ui_display_port_take_touch_activity(void);

/* Hides touch from LVGL while the panel is off; a gesture that wakes the panel stays hidden until released. */
void ui_display_port_set_input_enabled(bool enabled);
