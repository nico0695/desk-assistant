#pragma once

#include <stdint.h>

#include "connectivity_view.h"
#include "lvgl.h"
#include "settings_row.h"

/* Gestures only dispatch to these handlers; the caller runs the row transitions and calls ui_settings_show. */
typedef struct {
    void (*on_drag)(settings_id_t id, uint8_t value);
    /* Release or a lost gesture. */
    void (*on_release)(settings_id_t id);
    void (*on_select_timeout)(uint8_t seconds);
    /* The screen is being deleted, possibly mid-drag. */
    void (*on_closed)(void);
    lv_event_cb_t on_back;
    /* Opens the Wi-Fi screen. */
    lv_event_cb_t on_wifi;
} ui_settings_handlers_t;

/* Settings screen showing the rows; handlers must outlive the screen. */
lv_obj_t *ui_settings_screen_create(const settings_row_t rows[SETTINGS_COUNT], const ui_settings_handlers_t *handlers);

/* Refreshes the open Settings screen, if any, from the rows. */
void ui_settings_show(const settings_row_t rows[SETTINGS_COUNT]);

/* Short Wi-Fi state on the Wi-Fi button of the open Settings screen, if any. */
void ui_settings_show_wifi(connectivity_state_t state);
