#include "display_brightness.h"

uint8_t display_brightness_panel_value(unsigned percent)
{
    if (percent > DISPLAY_BRIGHTNESS_MAX_PERCENT) {
        percent = DISPLAY_BRIGHTNESS_MAX_PERCENT;
    }
    const unsigned rounding = DISPLAY_BRIGHTNESS_MAX_PERCENT / 2U;
    return (uint8_t)((percent * DISPLAY_BRIGHTNESS_MAX_PANEL_VALUE + rounding) / DISPLAY_BRIGHTNESS_MAX_PERCENT);
}
