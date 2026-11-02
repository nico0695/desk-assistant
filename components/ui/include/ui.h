#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "connectivity_view.h"
#include "esp_err.h"
#include "recording.h"
#include "settings_values.h"

/*
 * Starts the UI task, the only owner of LVGL, the panel and the touch, with the settings loaded at boot (copied).
 * Requires board_init.
 */
esp_err_t ui_start(const settings_loaded_t *settings);

/* Safe from any task; the latest reading is shown on the status bar. */
void ui_show_battery(bool known, uint8_t percent);

/* Safe from any task; old bridge generations never overwrite newer status. */
void ui_show_bridge_online(bool online, uint64_t generation);

/* A setting saved outside Settings (USB provisioning) is applied and shown as confirmed. Safe from any task. */
void ui_apply_stored_setting(settings_id_t id, uint8_t value);

/* Safe from any task and never blocks; the Wi-Fi screen and the Settings button show the latest view (copied). */
void ui_show_connectivity(const connectivity_view_t *view);

/* Safe from any task and never blocks; the recording panel and the screen hold follow the latest view (copied). */
void ui_show_recording(const recording_view_t *view);
