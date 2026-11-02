#include "ui_section.h"

#include "ui_text.h"

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
