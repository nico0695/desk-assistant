#pragma once

#include "lvgl.h"

typedef enum {
    UI_SECTION_TASKS,
    UI_SECTION_NOTES,
    UI_SECTION_ALERTS,
    UI_SECTION_SETTINGS,
    UI_SECTION_COUNT,
} ui_section_t;

const char *ui_section_title(ui_section_t section);
const char *ui_section_symbol(ui_section_t section);

/* Section screen with its bar and a Back button; the content stays empty until the section's stage. */
lv_obj_t *ui_section_screen_create(ui_section_t section, lv_event_cb_t on_back);
