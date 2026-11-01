#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

typedef struct {
    bool known;
    uint8_t percent;
} board_battery_reading_t;

/* Called from the battery task after each reading. */
typedef void (*board_battery_cb_t)(board_battery_reading_t reading, void *context);

/* Starts a low-priority task that reads the PMIC estimate now and then periodically. Requires board_init. */
esp_err_t board_battery_start(board_battery_cb_t on_reading, void *context);
