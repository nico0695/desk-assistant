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
