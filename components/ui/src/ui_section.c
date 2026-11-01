#include "ui_section.h"

#include "ui_status_bar.h"
#include "ui_text.h"
#include "ui_theme.h"
#include "ui_widgets.h"

typedef struct {
    const char *title;
    const char *symbol;
} section_presentation_t;

static const section_presentation_t SECTIONS[UI_SECTION_COUNT] = {
    [UI_SECTION_TASKS] = {UI_TEXT_TASKS, LV_SYMBOL_LIST},
    [UI_SECTION_NOTES] = {UI_TEXT_NOTES, LV_SYMBOL_EDIT},
    [UI_SECTION_ALERTS] = {UI_TEXT_ALERTS, LV_SYMBOL_BELL},
    [UI_SECTION_SETTINGS] = {UI_TEXT_SETTINGS, LV_SYMBOL_SETTINGS},
};

const char *ui_section_title(ui_section_t section)
{
    return SECTIONS[section].title;
}

const char *ui_section_symbol(ui_section_t section)
{
    return SECTIONS[section].symbol;
}

lv_obj_t *ui_section_screen_create(ui_section_t section, lv_event_cb_t on_back)
{
    lv_obj_t *screen = ui_screen_create();
    ui_status_bar_create(screen, ui_section_title(section));

    lv_obj_t *back = ui_button_create(screen, UI_BUTTON_STANDARD, LV_SYMBOL_LEFT, UI_TEXT_BACK);
    lv_obj_set_size(back, UI_CONTENT_WIDTH_PX, UI_BUTTON_HEIGHT_PX);
    lv_obj_align(back, LV_ALIGN_BOTTOM_MID, 0, -UI_SCREEN_MARGIN_PX);
    lv_obj_add_event_cb(back, on_back, LV_EVENT_CLICKED, NULL);
    return screen;
}
