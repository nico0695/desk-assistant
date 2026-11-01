#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Point in display coordinates. */
typedef struct {
    bool pressed;
    uint16_t x;
    uint16_t y;
} board_touch_sample_t;
