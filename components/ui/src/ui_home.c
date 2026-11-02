#include "ui_home.h"

#include "ui_status_bar.h"
#include "ui_text.h"
#include "ui_theme.h"
#include "ui_widgets.h"

#define GRID_COLUMNS 2
#define TILE_WIDTH_PX ((UI_CONTENT_WIDTH_PX - (GRID_COLUMNS - 1) * UI_GAP_PX) / GRID_COLUMNS)

static ui_open_section_cb_t open_section_callback;
/* Label of the record button while Home is shown; cleared when Home is deleted. */
static lv_obj_t *record_label;

static void forget_record_label(lv_event_t *event)
{
    (void)event;
    record_label = NULL;
}

static void open_section(lv_event_t *event)
{
    lv_obj_t *tile = lv_event_get_current_target(event);
    open_section_callback((ui_section_t)lv_obj_get_index(tile));
}

static void add_record_dot(lv_obj_t *button)
{
    lv_obj_t *dot = lv_obj_create(button);
    lv_obj_remove_style_all(dot);
    lv_obj_set_size(dot, UI_RECORD_DOT_SIZE_PX, UI_RECORD_DOT_SIZE_PX);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(dot, lv_color_hex(UI_COLOR_RECORD), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_remove_flag(dot, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_move_to_index(dot, 0);
}

static lv_obj_t *create_content(lv_obj_t *screen)
{
    lv_obj_t *content = lv_obj_create(screen);
    lv_obj_remove_style_all(content);
    lv_obj_set_size(content, UI_CONTENT_WIDTH_PX, LV_SIZE_CONTENT);
    lv_obj_align(content, LV_ALIGN_TOP_MID, 0, UI_STATUS_BAR_HEIGHT_PX + UI_GAP_PX);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_gap(content, UI_GAP_PX, LV_PART_MAIN);
    lv_obj_remove_flag(content, LV_OBJ_FLAG_SCROLLABLE);
    return content;
}

static void create_section_grid(lv_obj_t *content)
{
    lv_obj_t *grid = lv_obj_create(content);
    lv_obj_remove_style_all(grid);
    lv_obj_set_size(grid, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_gap(grid, UI_GAP_PX, LV_PART_MAIN);
    lv_obj_remove_flag(grid, LV_OBJ_FLAG_SCROLLABLE);

    /* Tiles are created in section order, so a tile's index is its section. */
    for (int section = 0; section < UI_SECTION_COUNT; ++section) {
        lv_obj_t *tile = ui_button_create(grid, UI_BUTTON_TILE, ui_section_symbol(section), ui_section_title(section));
        lv_obj_set_size(tile, TILE_WIDTH_PX, UI_TILE_HEIGHT_PX);
        lv_obj_add_event_cb(tile, open_section, LV_EVENT_CLICKED, NULL);
    }
}

void ui_home_show_record_entry(const char *text)
{
    if (record_label != NULL) {
        lv_label_set_text_static(record_label, text);
    }
}

lv_obj_t *ui_home_screen_create(ui_open_section_cb_t on_open_section, lv_event_cb_t on_record,
                                const char *record_text)
{
    open_section_callback = on_open_section;
    lv_obj_t *screen = ui_screen_create();
    ui_status_bar_create(screen, UI_TEXT_HOME_TITLE);

    lv_obj_t *content = create_content(screen);
    lv_obj_t *record = ui_button_create(content, UI_BUTTON_PRIMARY, NULL, record_text);
    lv_obj_set_size(record, lv_pct(100), UI_PRIMARY_BUTTON_HEIGHT_PX);
    lv_obj_set_style_text_font(record, ui_theme_font(UI_FONT_TITLE), LV_PART_MAIN);
    record_label = lv_obj_get_child(record, -1);
    lv_obj_add_event_cb(record_label, forget_record_label, LV_EVENT_DELETE, NULL);
    add_record_dot(record);
    lv_obj_add_event_cb(record, on_record, LV_EVENT_CLICKED, NULL);

    create_section_grid(content);
    return screen;
}
