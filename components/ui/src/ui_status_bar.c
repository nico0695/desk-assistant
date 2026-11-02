#include "ui_status_bar.h"

#include "ui_text.h"
#include "ui_theme.h"

#define BATTERY_FULL_MIN_PERCENT 90U
#define BATTERY_THREE_QUARTERS_MIN_PERCENT 65U
#define BATTERY_HALF_MIN_PERCENT 40U
#define BATTERY_LOW_MIN_PERCENT 15U

/* Diagonal across the Wi-Fi symbol of the body font. */
static const lv_point_precise_t WIFI_SLASH_POINTS[] = {{2, 26}, {30, 0}};

static ui_battery_t battery_reading;
static bool bridge_online;
static lv_obj_t *battery_icon;
static lv_obj_t *battery_text;
static lv_obj_t *wifi_slash;
static lv_obj_t *title_label;
static const char *base_title;
static const char *title_override;

static const char *battery_symbol(ui_battery_t battery)
{
    if (!battery.known) {
        return LV_SYMBOL_BATTERY_EMPTY;
    }
    if (battery.percent >= BATTERY_FULL_MIN_PERCENT) {
        return LV_SYMBOL_BATTERY_FULL;
    }
    if (battery.percent >= BATTERY_THREE_QUARTERS_MIN_PERCENT) {
        return LV_SYMBOL_BATTERY_3;
    }
    if (battery.percent >= BATTERY_HALF_MIN_PERCENT) {
        return LV_SYMBOL_BATTERY_2;
    }
    if (battery.percent >= BATTERY_LOW_MIN_PERCENT) {
        return LV_SYMBOL_BATTERY_1;
    }
    return LV_SYMBOL_BATTERY_EMPTY;
}

static void show_battery(void)
{
    if (battery_icon == NULL) {
        return;
    }
    lv_label_set_text_static(battery_icon, battery_symbol(battery_reading));
    if (battery_reading.known) {
        lv_label_set_text_fmt(battery_text, UI_TEXT_BATTERY_PERCENT_FORMAT, (unsigned)battery_reading.percent);
    } else {
        lv_label_set_text_static(battery_text, UI_TEXT_BATTERY_UNKNOWN);
    }
}

static void forget_deleted_bar(lv_event_t *event)
{
    if (battery_icon != NULL && lv_event_get_target(event) == lv_obj_get_parent(battery_icon)) {
        battery_icon = NULL;
        battery_text = NULL;
        wifi_slash = NULL;
        title_label = NULL;
    }
}

static void show_bridge_status(void)
{
    if (wifi_slash == NULL) {
        return;
    }
    if (bridge_online) {
        lv_obj_add_flag(wifi_slash, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_remove_flag(wifi_slash, LV_OBJ_FLAG_HIDDEN);
    }
}

static lv_obj_t *create_wifi_icon(lv_obj_t *parent)
{
    lv_obj_t *container = lv_obj_create(parent);
    lv_obj_remove_style_all(container);
    lv_obj_set_size(container, LV_SIZE_CONTENT, LV_SIZE_CONTENT);

    lv_obj_t *icon = lv_label_create(container);
    lv_label_set_text_static(icon, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(icon, lv_color_hex(UI_COLOR_TEXT_SECONDARY), LV_PART_MAIN);

    wifi_slash = lv_line_create(container);
    lv_line_set_points(wifi_slash, WIFI_SLASH_POINTS, sizeof(WIFI_SLASH_POINTS) / sizeof(WIFI_SLASH_POINTS[0]));
    lv_obj_set_style_line_color(wifi_slash, lv_color_hex(UI_COLOR_TEXT), LV_PART_MAIN);
    lv_obj_set_style_line_width(wifi_slash, UI_WIFI_SLASH_WIDTH_PX, LV_PART_MAIN);
    lv_obj_set_style_line_rounded(wifi_slash, true, LV_PART_MAIN);
    show_bridge_status();
    return container;
}

lv_obj_t *ui_status_bar_create(lv_obj_t *screen, const char *title)
{
    lv_obj_t *bar = lv_obj_create(screen);
    lv_obj_remove_style_all(bar);
    lv_obj_set_size(bar, lv_pct(100), UI_STATUS_BAR_HEIGHT_PX);
    lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_pad_hor(bar, UI_STATUS_BAR_SIDE_INSET_PX, LV_PART_MAIN);
    lv_obj_set_style_pad_gap(bar, UI_ICON_TEXT_GAP_PX, LV_PART_MAIN);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    title_label = lv_label_create(bar);
    base_title = title;
    lv_label_set_text_static(title_label, title_override != NULL ? title_override : base_title);
    lv_obj_set_style_text_font(title_label, ui_theme_font(UI_FONT_TITLE), LV_PART_MAIN);
    lv_obj_set_flex_grow(title_label, 1);

    create_wifi_icon(bar);

    battery_icon = lv_label_create(bar);
    battery_text = lv_label_create(bar);
    lv_obj_add_style(battery_text, &ui_theme_styles()->text_secondary, LV_PART_MAIN);
    lv_obj_add_event_cb(bar, forget_deleted_bar, LV_EVENT_DELETE, NULL);
    show_battery();
    return bar;
}

void ui_status_bar_set_battery(ui_battery_t battery)
{
    battery_reading = battery;
    show_battery();
}

void ui_status_bar_set_bridge_online(bool online)
{
    bridge_online = online;
    show_bridge_status();
}

void ui_status_bar_set_title_override(const char *title)
{
    title_override = title;
    if (title_label != NULL) lv_label_set_text_static(title_label, title != NULL ? title : base_title);
}

void ui_status_bar_set_title(const char *title)
{
    base_title = title;
    if (title_label != NULL && title_override == NULL) lv_label_set_text_static(title_label, title);
}
