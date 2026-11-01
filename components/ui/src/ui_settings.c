#include "ui_settings.h"

#include <stdio.h>

#include "ui_settings_text.h"
#include "ui_status_bar.h"
#include "ui_text.h"
#include "ui_theme.h"
#include "ui_widgets.h"
#include "ui_wifi_text.h"

#define VALUE_TEXT_CAPACITY 16
#define TIMEOUT_BUTTON_WIDTH_PX                                                                                       \
    ((UI_CONTENT_WIDTH_PX - (SETTINGS_SCREEN_TIMEOUT_OPTION_COUNT - 1) * UI_ROW_LABEL_GAP_PX) /                       \
     SETTINGS_SCREEN_TIMEOUT_OPTION_COUNT)
#define BACK_WITH_WIFI_WIDTH_PX 168
#define SLIDER_WIDTH_PX (UI_CONTENT_WIDTH_PX - UI_SLIDER_KNOB_SIZE_PX)
#define SLIDER_TOUCH_MARGIN_PX ((UI_SLIDER_TOUCH_HEIGHT_PX - UI_SLIDER_TRACK_HEIGHT_PX) / 2)

typedef struct {
    lv_obj_t *status;
    lv_obj_t *slider;
} row_widgets_t;

/* Widgets of the open screen; cleared when it is deleted. */
static const ui_settings_handlers_t *handlers;
static lv_obj_t *screen_object;
static row_widgets_t widgets[SETTINGS_COUNT];
static lv_obj_t *timeout_buttons[SETTINGS_SCREEN_TIMEOUT_OPTION_COUNT];
static lv_obj_t *wifi_label;
static char timeout_texts[SETTINGS_SCREEN_TIMEOUT_OPTION_COUNT][VALUE_TEXT_CAPACITY];

static const char *const ROW_TITLES[SETTINGS_COUNT] = {
    [SETTINGS_SCREEN_TIMEOUT] = UI_TEXT_SETTINGS_SCREEN_OFF,
    [SETTINGS_VOLUME] = UI_TEXT_SETTINGS_VOLUME,
    [SETTINGS_BRIGHTNESS] = UI_TEXT_SETTINGS_BRIGHTNESS,
};

static const char *error_text(settings_row_error_t error)
{
    switch (error) {
    case SETTINGS_ROW_ERROR_LOAD:
        return UI_TEXT_SETTINGS_LOAD_FAILED;
    case SETTINGS_ROW_ERROR_APPLY:
        return UI_TEXT_SETTINGS_NOT_APPLIED;
    case SETTINGS_ROW_ERROR_SAVE:
        return UI_TEXT_SETTINGS_NOT_SAVED;
    case SETTINGS_ROW_ERROR_NONE:
    default:
        return NULL;
    }
}

static settings_id_t row_id(lv_event_t *event)
{
    return (settings_id_t)(uintptr_t)lv_event_get_user_data(event);
}

static void dispatch_drag(lv_event_t *event)
{
    lv_obj_t *slider = lv_event_get_target_obj(event);
    handlers->on_drag(row_id(event), (uint8_t)lv_slider_get_value(slider));
}

static void dispatch_release(lv_event_t *event)
{
    handlers->on_release(row_id(event));
}

static void dispatch_timeout(lv_event_t *event)
{
    const uintptr_t option = (uintptr_t)lv_event_get_user_data(event);
    handlers->on_select_timeout(SETTINGS_SCREEN_TIMEOUT_OPTIONS_S[option]);
}

static void forget_screen(lv_event_t *event)
{
    /* A screen replaced after its successor was created must not clear the successor's state. */
    if (lv_event_get_target(event) != screen_object) {
        return;
    }
    const ui_settings_handlers_t *closing = handlers;
    handlers = NULL;
    screen_object = NULL;
    for (unsigned i = 0; i < SETTINGS_COUNT; ++i) {
        widgets[i] = (row_widgets_t){0};
    }
    for (unsigned i = 0; i < SETTINGS_SCREEN_TIMEOUT_OPTION_COUNT; ++i) {
        timeout_buttons[i] = NULL;
    }
    wifi_label = NULL;
    closing->on_closed();
}

static lv_obj_t *create_box(lv_obj_t *parent, lv_flex_flow_t flow, int32_t gap)
{
    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_remove_style_all(box);
    lv_obj_set_size(box, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(box, flow);
    lv_obj_set_style_pad_gap(box, gap, LV_PART_MAIN);
    lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    return box;
}

/* Title on the left and the value or error on the right. */
static void create_row_header(lv_obj_t *row, settings_id_t id)
{
    lv_obj_t *header = create_box(row, LV_FLEX_FLOW_ROW, 0);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t *title = lv_label_create(header);
    lv_label_set_text_static(title, ROW_TITLES[id]);
    widgets[id].status = lv_label_create(header);
}

static void create_timeout_buttons(lv_obj_t *row)
{
    lv_obj_t *options = create_box(row, LV_FLEX_FLOW_ROW, UI_ROW_LABEL_GAP_PX);
    ui_styles_t *styles = ui_theme_styles();
    for (unsigned i = 0; i < SETTINGS_SCREEN_TIMEOUT_OPTION_COUNT; ++i) {
        const uint8_t seconds = SETTINGS_SCREEN_TIMEOUT_OPTIONS_S[i];
        if (seconds == SETTINGS_SCREEN_TIMEOUT_ALWAYS_ON_S) {
            snprintf(timeout_texts[i], sizeof(timeout_texts[i]), "%s", UI_TEXT_SETTINGS_ALWAYS_ON);
        } else {
            snprintf(timeout_texts[i], sizeof(timeout_texts[i]), UI_TEXT_SETTINGS_TIMEOUT_FORMAT, (unsigned)seconds);
        }
        lv_obj_t *button = ui_button_create(options, UI_BUTTON_STANDARD, NULL, timeout_texts[i]);
        lv_obj_set_size(button, TIMEOUT_BUTTON_WIDTH_PX, UI_BUTTON_HEIGHT_PX);
        lv_obj_add_style(button, &styles->button_primary, LV_PART_MAIN | LV_STATE_CHECKED);
        lv_obj_add_event_cb(button, dispatch_timeout, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
        timeout_buttons[i] = button;
    }
}

static void create_slider(lv_obj_t *row, settings_id_t id, int32_t minimum, int32_t maximum)
{
    ui_styles_t *styles = ui_theme_styles();
    lv_obj_t *holder = create_box(row, LV_FLEX_FLOW_ROW, 0);
    lv_obj_set_height(holder, UI_SLIDER_KNOB_SIZE_PX);
    lv_obj_set_flex_align(holder, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *slider = lv_slider_create(holder);
    lv_obj_remove_style_all(slider);
    lv_obj_add_style(slider, &styles->slider_track, LV_PART_MAIN);
    lv_obj_add_style(slider, &styles->slider_indicator, LV_PART_INDICATOR);
    lv_obj_add_style(slider, &styles->slider_knob, LV_PART_KNOB);
    lv_obj_set_size(slider, SLIDER_WIDTH_PX, UI_SLIDER_TRACK_HEIGHT_PX);
    lv_obj_set_ext_click_area(slider, SLIDER_TOUCH_MARGIN_PX);
    lv_slider_set_range(slider, minimum, maximum);
    void *user_data = (void *)(uintptr_t)id;
    lv_obj_add_event_cb(slider, dispatch_drag, LV_EVENT_VALUE_CHANGED, user_data);
    lv_obj_add_event_cb(slider, dispatch_release, LV_EVENT_RELEASED, user_data);
    lv_obj_add_event_cb(slider, dispatch_release, LV_EVENT_PRESS_LOST, user_data);
    widgets[id].slider = slider;
}

static lv_obj_t *create_row(lv_obj_t *content, settings_id_t id)
{
    lv_obj_t *row = create_box(content, LV_FLEX_FLOW_COLUMN, UI_ROW_LABEL_GAP_PX);
    create_row_header(row, id);
    return row;
}

static void show_row(const settings_row_t *row)
{
    const row_widgets_t *row_widgets = &widgets[row->id];
    const char *error = error_text(settings_row_error(row));
    lv_obj_set_style_text_color(row_widgets->status,
                                lv_color_hex(error != NULL ? UI_COLOR_ERROR : UI_COLOR_TEXT_SECONDARY), LV_PART_MAIN);
    if (error != NULL) {
        lv_label_set_text_static(row_widgets->status, error);
    } else if (row_widgets->slider != NULL) {
        lv_label_set_text_fmt(row_widgets->status, UI_TEXT_SETTINGS_PERCENT_FORMAT, (unsigned)row->shown);
    } else {
        lv_label_set_text_static(row_widgets->status, "");
    }

    if (row_widgets->slider != NULL) {
        lv_slider_set_value(row_widgets->slider, row->shown, LV_ANIM_OFF);
        return;
    }
    for (unsigned i = 0; i < SETTINGS_SCREEN_TIMEOUT_OPTION_COUNT; ++i) {
        lv_obj_set_state(timeout_buttons[i], LV_STATE_CHECKED, SETTINGS_SCREEN_TIMEOUT_OPTIONS_S[i] == row->shown);
    }
}

static void create_bottom_buttons(lv_obj_t *screen)
{
    const bool with_wifi = handlers->on_wifi != NULL;
    lv_obj_t *back = ui_button_create(screen, UI_BUTTON_STANDARD, LV_SYMBOL_LEFT, UI_TEXT_BACK);
    lv_obj_set_size(back, with_wifi ? BACK_WITH_WIFI_WIDTH_PX : UI_CONTENT_WIDTH_PX, UI_BUTTON_HEIGHT_PX);
    lv_obj_align(back, LV_ALIGN_BOTTOM_LEFT, UI_SCREEN_MARGIN_PX, -UI_SCREEN_MARGIN_PX);
    lv_obj_add_event_cb(back, handlers->on_back, LV_EVENT_CLICKED, NULL);
    if (!with_wifi) {
        return;
    }
    lv_obj_t *wifi = ui_button_create(screen, UI_BUTTON_STANDARD, LV_SYMBOL_WIFI, UI_TEXT_WIFI_SHORT_NO_NETWORKS);
    lv_obj_set_size(wifi, UI_CONTENT_WIDTH_PX - UI_GAP_PX - BACK_WITH_WIFI_WIDTH_PX, UI_BUTTON_HEIGHT_PX);
    lv_obj_align(wifi, LV_ALIGN_BOTTOM_RIGHT, -UI_SCREEN_MARGIN_PX, -UI_SCREEN_MARGIN_PX);
    lv_obj_add_event_cb(wifi, handlers->on_wifi, LV_EVENT_CLICKED, NULL);
    wifi_label = lv_obj_get_child(wifi, -1);
}

lv_obj_t *ui_settings_screen_create(const settings_row_t rows[SETTINGS_COUNT], const ui_settings_handlers_t *on_events)
{
    handlers = on_events;
    lv_obj_t *screen = ui_screen_create();
    screen_object = screen;
    ui_status_bar_create(screen, UI_TEXT_SETTINGS);

    lv_obj_t *content = create_box(screen, LV_FLEX_FLOW_COLUMN, UI_SETTINGS_ROW_GAP_PX);
    lv_obj_set_width(content, UI_CONTENT_WIDTH_PX);
    lv_obj_align(content, LV_ALIGN_TOP_MID, 0, UI_STATUS_BAR_HEIGHT_PX + UI_SETTINGS_ROW_GAP_PX);

    create_timeout_buttons(create_row(content, SETTINGS_SCREEN_TIMEOUT));
    create_slider(create_row(content, SETTINGS_VOLUME), SETTINGS_VOLUME, SETTINGS_VOLUME_MIN_PERCENT,
                  SETTINGS_VOLUME_MAX_PERCENT);
    create_slider(create_row(content, SETTINGS_BRIGHTNESS), SETTINGS_BRIGHTNESS, SETTINGS_BRIGHTNESS_MIN_PERCENT,
                  SETTINGS_BRIGHTNESS_MAX_PERCENT);

    create_bottom_buttons(screen);

    lv_obj_add_event_cb(screen, forget_screen, LV_EVENT_DELETE, NULL);
    ui_settings_show(rows);
    return screen;
}

void ui_settings_show(const settings_row_t rows[SETTINGS_COUNT])
{
    if (handlers == NULL) {
        return;
    }
    for (unsigned i = 0; i < SETTINGS_COUNT; ++i) {
        show_row(&rows[i]);
    }
}

void ui_settings_show_wifi(connectivity_state_t state)
{
    if (wifi_label == NULL) {
        return;
    }
    switch (state) {
    case CONNECTIVITY_CONNECTED:
        lv_label_set_text_static(wifi_label, UI_TEXT_WIFI_SHORT_CONNECTED);
        break;
    case CONNECTIVITY_SEARCHING:
        lv_label_set_text_static(wifi_label, UI_TEXT_WIFI_SHORT_SEARCHING);
        break;
    case CONNECTIVITY_OFFLINE:
        lv_label_set_text_static(wifi_label, UI_TEXT_WIFI_SHORT_OFFLINE);
        break;
    case CONNECTIVITY_NO_NETWORKS:
    default:
        lv_label_set_text_static(wifi_label, UI_TEXT_WIFI_SHORT_NO_NETWORKS);
        break;
    }
}
