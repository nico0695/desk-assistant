#pragma once

#include "lvgl.h"
#include "ui_section.h"

typedef void (*ui_open_section_cb_t)(ui_section_t section);

/* Home screen: the record entry, labeled record_text (a static string), and the section grid. */
lv_obj_t *ui_home_screen_create(ui_open_section_cb_t on_open_section, lv_event_cb_t on_record,
                                const char *record_text);

/* Relabels the record entry while Home is shown; text must be static. UI task only. */
void ui_home_show_record_entry(const char *text);
