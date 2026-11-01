#pragma once

#include "driver/i2c_master.h"

/* Shared I2C bus created by board_init; every device adds itself once and keeps its handle. */
i2c_master_bus_handle_t board_bus(void);

#define BOARD_I2C_TIMEOUT_MS 100
