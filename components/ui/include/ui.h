#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "settings_values.h"

/*
 * Starts the UI task, the only owner of LVGL, the panel and the touch, with the settings loaded at boot (copied).
 * Requires board_init.
 */
esp_err_t ui_start(const settings_loaded_t *settings);

/* Safe from any task; the latest reading is shown on the status bar. */
void ui_show_battery(bool known, uint8_t percent);

/* A setting saved outside Settings (USB provisioning) is applied and shown as confirmed. Safe from any task. */
void ui_apply_stored_setting(settings_id_t id, uint8_t value);
