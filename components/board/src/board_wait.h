#pragma once

#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* vTaskDelay(n) can return up to one tick early, so add a tick to guarantee the minimum. */
static inline void board_wait_at_least_ms(uint32_t milliseconds)
{
    vTaskDelay(pdMS_TO_TICKS(milliseconds) + 1);
}
