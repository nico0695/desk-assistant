#include "ui_theme.h"

#include "ui_fonts.h"

static lv_font_t secondary_font;
static lv_font_t body_font;
static lv_font_t title_font;
static ui_styles_t styles;

static void init_role_font(lv_font_t *role_font, const lv_font_t *base, const lv_font_t *latin1_fallback)
{
    *role_font = *base;
    role_font->fallback = latin1_fallback;
}

static void init_button_style(lv_style_t *style, uint32_t color)
{
    lv_style_init(style);
    lv_style_set_bg_color(style, lv_color_hex(color));
    lv_style_set_bg_opa(style, LV_OPA_COVER);
    lv_style_set_radius(style, UI_CORNER_RADIUS_PX);
    lv_style_set_border_width(style, 0);
    lv_style_set_text_color(style, lv_color_hex(UI_COLOR_TEXT));
    lv_style_set_text_font(style, &body_font);
}

static void init_pressed_style(lv_style_t *style, uint32_t color)
{
    lv_style_init(style);
    lv_style_set_bg_color(style, lv_color_hex(color));
}

void ui_theme_init(void)
{
    init_role_font(&secondary_font, &lv_font_montserrat_20, &ui_font_latin1_20);
    init_role_font(&body_font, &lv_font_montserrat_24, &ui_font_latin1_24);
    init_role_font(&title_font, &lv_font_montserrat_28, &ui_font_latin1_28);

    lv_style_init(&styles.screen);
    lv_style_set_bg_color(&styles.screen, lv_color_hex(UI_COLOR_BACKGROUND));
    lv_style_set_bg_opa(&styles.screen, LV_OPA_COVER);
    lv_style_set_text_color(&styles.screen, lv_color_hex(UI_COLOR_TEXT));
    lv_style_set_text_font(&styles.screen, &body_font);

    init_button_style(&styles.button, UI_COLOR_CONTROL);
    init_pressed_style(&styles.button_pressed, UI_COLOR_CONTROL_PRESSED);
    init_button_style(&styles.button_primary, UI_COLOR_PRIMARY);
    init_pressed_style(&styles.button_primary_pressed, UI_COLOR_PRIMARY_PRESSED);
    lv_style_init(&styles.button_disabled);
    lv_style_set_bg_color(&styles.button_disabled, lv_color_hex(UI_COLOR_SURFACE_RAISED));
    lv_style_set_text_color(&styles.button_disabled, lv_color_hex(UI_COLOR_TEXT_SECONDARY));

    lv_style_init(&styles.text_secondary);
    lv_style_set_text_color(&styles.text_secondary, lv_color_hex(UI_COLOR_TEXT_SECONDARY));
    lv_style_set_text_font(&styles.text_secondary, &secondary_font);

    lv_style_init(&styles.slider_track);
    lv_style_set_bg_color(&styles.slider_track, lv_color_hex(UI_COLOR_CONTROL));
    lv_style_set_bg_opa(&styles.slider_track, LV_OPA_COVER);
    lv_style_set_radius(&styles.slider_track, LV_RADIUS_CIRCLE);

    lv_style_init(&styles.slider_indicator);
    lv_style_set_bg_color(&styles.slider_indicator, lv_color_hex(UI_COLOR_PRIMARY_PRESSED));
    lv_style_set_bg_opa(&styles.slider_indicator, LV_OPA_COVER);
    lv_style_set_radius(&styles.slider_indicator, LV_RADIUS_CIRCLE);

    lv_style_init(&styles.slider_knob);
    lv_style_set_bg_color(&styles.slider_knob, lv_color_hex(UI_COLOR_TEXT));
    lv_style_set_bg_opa(&styles.slider_knob, LV_OPA_COVER);
    lv_style_set_radius(&styles.slider_knob, LV_RADIUS_CIRCLE);
    lv_style_set_pad_all(&styles.slider_knob, (UI_SLIDER_KNOB_SIZE_PX - UI_SLIDER_TRACK_HEIGHT_PX) / 2);
}

const lv_font_t *ui_theme_font(ui_font_role_t role)
{
    switch (role) {
    case UI_FONT_SECONDARY:
        return &secondary_font;
    case UI_FONT_TITLE:
        return &title_font;
    case UI_FONT_BODY:
    default:
        return &body_font;
    }
}

ui_styles_t *ui_theme_styles(void)
{
    return &styles;
}
