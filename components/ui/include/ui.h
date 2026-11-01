#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

/* Starts the UI task, the only owner of LVGL, the panel and the touch. Requires board_init. */
esp_err_t ui_start(void);

/* Safe from any task; the latest reading is shown on the status bar. */
void ui_show_battery(bool known, uint8_t percent);
