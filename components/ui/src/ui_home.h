#pragma once

#include "lvgl.h"
#include "ui_section.h"

typedef void (*ui_open_section_cb_t)(ui_section_t section);

/* Home screen: Record and the section grid. */
lv_obj_t *ui_home_screen_create(ui_open_section_cb_t on_open_section, lv_event_cb_t on_record);
