#pragma once

#include <stdint.h>

#define DISPLAY_BRIGHTNESS_MAX_PERCENT 100U
#define DISPLAY_BRIGHTNESS_MAX_PANEL_VALUE 255U

/* Linear and rounded; values above the maximum are clamped. Callers enforce the readable floor. */
uint8_t display_brightness_panel_value(unsigned percent);
