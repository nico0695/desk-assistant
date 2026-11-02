#include "ui_widgets.h"

#include "ui_theme.h"

#define DIALOG_WIDTH_PX 400
#define DIALOG_PAD_PX 24
#define DIALOG_INNER_WIDTH_PX (DIALOG_WIDTH_PX - 2 * DIALOG_PAD_PX)
#define DIALOG_BUTTON_WIDTH_PX ((DIALOG_INNER_WIDTH_PX - UI_GAP_PX) / 2)
#define DIALOG_SCRIM_OPA LV_OPA_70

lv_obj_t *ui_screen_create(void)
{
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_add_style(screen, &ui_theme_styles()->screen, LV_PART_MAIN);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    return screen;
}

static int32_t minimum_height(ui_button_kind_t kind)
{
    switch (kind) {
    case UI_BUTTON_PRIMARY:
        return UI_PRIMARY_BUTTON_HEIGHT_PX;
    case UI_BUTTON_TILE:
        return UI_TILE_HEIGHT_PX;
    case UI_BUTTON_STANDARD:
    default:
        return UI_BUTTON_HEIGHT_PX;
    }
}

lv_obj_t *ui_button_create(lv_obj_t *parent, ui_button_kind_t kind, const char *symbol, const char *text)
{
    ui_styles_t *styles = ui_theme_styles();
    const bool primary = kind == UI_BUTTON_PRIMARY;

    lv_obj_t *button = lv_button_create(parent);
    lv_obj_add_style(button, primary ? &styles->button_primary : &styles->button, LV_PART_MAIN);
    lv_obj_add_style(button, primary ? &styles->button_primary_pressed : &styles->button_pressed,
                     LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_add_style(button, &styles->button_disabled, LV_PART_MAIN | LV_STATE_DISABLED);
    lv_obj_set_style_min_height(button, minimum_height(kind), LV_PART_MAIN);
    lv_obj_remove_flag(button, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_flex_flow(button, kind == UI_BUTTON_TILE ? LV_FLEX_FLOW_COLUMN : LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(button, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(button, UI_ICON_TEXT_GAP_PX, LV_PART_MAIN);

    if (symbol != NULL) {
        lv_obj_t *icon = lv_label_create(button);
        lv_label_set_text_static(icon, symbol);
    }
    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text_static(label, text);
    return button;
}

lv_obj_t *ui_box_create(lv_obj_t *parent, lv_flex_flow_t flow, int32_t gap)
{
    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_remove_style_all(box);
    lv_obj_set_size(box, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(box, flow);
    lv_obj_set_style_pad_gap(box, gap, LV_PART_MAIN);
    lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    return box;
}

lv_obj_t *ui_scroll_box_create(lv_obj_t *parent, int32_t width, int32_t height)
{
    lv_obj_t *box = ui_box_create(parent, LV_FLEX_FLOW_COLUMN, UI_GAP_PX);
    lv_obj_set_size(box, width, height);
    lv_obj_set_flex_align(box, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(box, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(box, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_width(box, UI_SCROLLBAR_WIDTH_PX, LV_PART_SCROLLBAR);
    lv_obj_set_style_radius(box, UI_SCROLLBAR_WIDTH_PX / 2, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_color(box, lv_color_hex(UI_COLOR_CONTROL_PRESSED), LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, LV_PART_SCROLLBAR);
    lv_obj_set_style_pad_right(box, 2 * UI_SCROLLBAR_WIDTH_PX, LV_PART_MAIN);
    return box;
}

static lv_obj_t *create_dialog_label(lv_obj_t *panel, const char *text)
{
    lv_obj_t *label = lv_label_create(panel);
    lv_label_set_text(label, text);
    lv_obj_set_width(label, DIALOG_INNER_WIDTH_PX);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    return label;
}

lv_obj_t *ui_dialog_create(lv_obj_t *parent, const char *title, const char *body)
{
    lv_obj_t *scrim = lv_obj_create(parent);
    lv_obj_remove_style_all(scrim);
    lv_obj_set_size(scrim, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(scrim, lv_color_hex(UI_COLOR_BACKGROUND), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scrim, DIALOG_SCRIM_OPA, LV_PART_MAIN);
    lv_obj_remove_flag(scrim, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *panel = lv_obj_create(scrim);
    lv_obj_remove_style_all(panel);
    lv_obj_set_size(panel, DIALOG_WIDTH_PX, LV_SIZE_CONTENT);
    lv_obj_center(panel);
    lv_obj_set_style_bg_color(panel, lv_color_hex(UI_COLOR_SURFACE_RAISED), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(panel, UI_CORNER_RADIUS_PX, LV_PART_MAIN);
    lv_obj_set_style_pad_all(panel, DIALOG_PAD_PX, LV_PART_MAIN);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_gap(panel, UI_GAP_PX, LV_PART_MAIN);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title_label = create_dialog_label(panel, title);
    lv_obj_set_style_text_font(title_label, ui_theme_font(UI_FONT_TITLE), LV_PART_MAIN);
    lv_obj_t *body_label = create_dialog_label(panel, body);
    lv_obj_add_style(body_label, &ui_theme_styles()->text_secondary, LV_PART_MAIN);
    return panel;
}

lv_obj_t *ui_dialog_buttons_create(lv_obj_t *panel)
{
    return ui_box_create(panel, LV_FLEX_FLOW_ROW_WRAP, UI_GAP_PX);
}

lv_obj_t *ui_dialog_button_create(lv_obj_t *buttons, ui_button_kind_t kind, const char *text, lv_event_cb_t on_click,
                                  void *user_data)
{
    lv_obj_t *button = ui_button_create(buttons, kind, NULL, text);
    lv_obj_set_style_min_height(button, UI_BUTTON_HEIGHT_PX, LV_PART_MAIN);
    lv_obj_set_size(button, DIALOG_BUTTON_WIDTH_PX, UI_BUTTON_HEIGHT_PX);
    lv_obj_add_event_cb(button, on_click, LV_EVENT_CLICKED, user_data);
    return button;
}
