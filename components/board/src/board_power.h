#pragma once

#include "board_battery.h"
#include "esp_err.h"

/* Verifies the AXP2101 identity and pulses ALDO3, which drives the panel reset. Touches only those bits. */
esp_err_t board_power_init(void);

/* Read-only estimate: known only with a valid identity, battery detection enabled, presence and range. */
esp_err_t board_power_read_battery(board_battery_reading_t *reading);
