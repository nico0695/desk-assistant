#include "ui_widgets.h"

#include "ui_theme.h"

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
