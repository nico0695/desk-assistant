#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

typedef struct {
    bool known;
    uint8_t percent;
} ui_battery_t;

/* Adds the bar to the top of a screen. The most recently created bar receives updates; create it on the screen about to be loaded. */
lv_obj_t *ui_status_bar_create(lv_obj_t *screen, const char *title);

/* Shows the reading on the most recent bar and keeps it for bars created later. */
void ui_status_bar_set_battery(ui_battery_t battery);

/* The UI owner updates the newest bar and retains the state for later bars. */
void ui_status_bar_set_bridge_online(bool online);
/* UI owner only; NULL restores the active screen's title after an overlay closes. */
void ui_status_bar_set_title_override(const char *title);
/* Replaces the title of the newest bar; the text must outlive it. An overlay override keeps priority. */
void ui_status_bar_set_title(const char *title);
