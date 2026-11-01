#include "ui_wifi.h"

#include <stdio.h>
#include <string.h>

#include "ui_status_bar.h"
#include "ui_text.h"
#include "ui_theme.h"
#include "ui_widgets.h"
#include "ui_wifi_text.h"

#define WIFI_STATE_Y_PX (UI_STATUS_BAR_HEIGHT_PX + UI_GAP_PX / 2)
#define WIFI_STATE_HEIGHT_PX 32
#define WIFI_ROW_HEIGHT_PX 56
#define WIFI_ROW_PAD_PX 16
#define WIFI_ROW_GAP_PX 8
#define WIFI_BOTTOM_Y_PX (BOARD_DISPLAY_HEIGHT_PX - UI_SCREEN_MARGIN_PX - UI_BUTTON_HEIGHT_PX)
#define WIFI_LIST_Y_PX (WIFI_STATE_Y_PX + WIFI_STATE_HEIGHT_PX + UI_ROW_LABEL_GAP_PX)
#define WIFI_LIST_HEIGHT_PX (WIFI_BOTTOM_Y_PX - UI_GAP_PX - WIFI_LIST_Y_PX)
#define WIFI_ACTION_WIDTH_PX (UI_CONTENT_WIDTH_PX - UI_GAP_PX - UI_BACK_WITH_ACTION_WIDTH_PX)
#define WIFI_SETUP_TOP_Y_PX (UI_STATUS_BAR_HEIGHT_PX + UI_GAP_PX)
#define WIFI_QR_SIZE_PX 168
#define WIFI_QR_QUIET_ZONE_PX 8
#define WIFI_QR_BOX_PX (WIFI_QR_SIZE_PX + 2 * WIFI_QR_QUIET_ZONE_PX)
#define WIFI_SETUP_TEXT_X_PX (UI_SCREEN_MARGIN_PX + WIFI_QR_BOX_PX + UI_GAP_PX)
#define WIFI_SETUP_TEXT_WIDTH_PX (BOARD_DISPLAY_WIDTH_PX - UI_SCREEN_MARGIN_PX - WIFI_SETUP_TEXT_X_PX)
#define WIFI_DIALOG_WIDTH_PX 400
#define WIFI_DIALOG_PAD_PX 24
#define WIFI_DIALOG_INNER_WIDTH_PX (WIFI_DIALOG_WIDTH_PX - 2 * WIFI_DIALOG_PAD_PX)
#define WIFI_DIALOG_BUTTON_WIDTH_PX ((WIFI_DIALOG_INNER_WIDTH_PX - UI_GAP_PX) / 2)
#define WIFI_DIALOG_SCRIM_OPA LV_OPA_70
#define WIFI_QR_TEXT_CAPACITY (sizeof(UI_TEXT_WIFI_SETUP_QR_FORMAT) + SETUP_AP_SSID_SIZE + SETUP_AP_PASSWORD_SIZE)

/* State of the open screen; cleared when it is deleted. */
static const ui_wifi_handlers_t *handlers;
static connectivity_view_t shown;
static lv_obj_t *screen_object;
static lv_obj_t *content;
static lv_obj_t *dialog;
static lv_obj_t *network_list;
static ui_wifi_dialog_t dialog_kind;
static char dialog_ssid[WIFI_SSID_SIZE];


static lv_obj_t *create_label(lv_obj_t *parent, const char *text, uint32_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_hex(color), LV_PART_MAIN);
    return label;
}

static lv_obj_t *create_secondary_label(lv_obj_t *parent, const char *text)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_add_style(label, &ui_theme_styles()->text_secondary, LV_PART_MAIN);
    return label;
}

/* Primary buttons are taller by default; every button on this screen is a standard row. */
static lv_obj_t *create_button(lv_obj_t *parent, ui_button_kind_t kind, const char *symbol, const char *text)
{
    lv_obj_t *button = ui_button_create(parent, kind, symbol, text);
    lv_obj_set_style_min_height(button, UI_BUTTON_HEIGHT_PX, LV_PART_MAIN);
    return button;
}

static lv_obj_t *create_fixed_button(lv_obj_t *parent, ui_button_kind_t kind, const char *symbol, const char *text,
                                     int32_t x, int32_t width, lv_event_cb_t on_click)
{
    lv_obj_t *button = create_button(parent, kind, symbol, text);
    lv_obj_set_size(button, width, UI_BUTTON_HEIGHT_PX);
    lv_obj_set_pos(button, x, WIFI_BOTTOM_Y_PX);
    lv_obj_add_event_cb(button, on_click, LV_EVENT_CLICKED, NULL);
    return button;
}

static void close_dialog(void)
{
    if (dialog != NULL) {
        lv_obj_delete(dialog);
        dialog = NULL;
    }
}

static void on_dialog_cancel(lv_event_t *event)
{
    (void)event;
    close_dialog();
}

static void on_forget_confirmed(lv_event_t *event)
{
    (void)event;
    char ssid[WIFI_SSID_SIZE];
    snprintf(ssid, sizeof(ssid), "%s", dialog_ssid);
    close_dialog();
    handlers->on_forget(ssid);
}

static void on_add_confirmed(lv_event_t *event)
{
    (void)event;
    close_dialog();
    handlers->on_add();
}

static void on_add_requested(lv_event_t *event)
{
    (void)event;
    if (shown.state == CONNECTIVITY_CONNECTED) {
        ui_wifi_open_dialog(UI_WIFI_DIALOG_ADD, NULL);
    } else {
        handlers->on_add();
    }
}

static void on_cancel_setup(lv_event_t *event)
{
    (void)event;
    handlers->on_cancel_setup();
}

static void on_row_tapped(lv_event_t *event)
{
    const uintptr_t index = (uintptr_t)lv_event_get_user_data(event);
    ui_wifi_open_dialog(UI_WIFI_DIALOG_FORGET, shown.saved[index]);
}

static const char *state_text(const connectivity_view_t *view, char *buffer, size_t capacity)
{
    switch (view->state) {
    case CONNECTIVITY_CONNECTED:
        snprintf(buffer, capacity, UI_TEXT_WIFI_STATE_CONNECTED_FORMAT, view->saved[view->connected_index]);
        return buffer;
    case CONNECTIVITY_SEARCHING:
        return UI_TEXT_WIFI_STATE_SEARCHING;
    case CONNECTIVITY_OFFLINE:
        return UI_TEXT_WIFI_STATE_OFFLINE;
    case CONNECTIVITY_NO_NETWORKS:
    default:
        return UI_TEXT_WIFI_STATE_NO_NETWORKS;
    }
}

static uint32_t state_color(const connectivity_view_t *view)
{
    switch (view->state) {
    case CONNECTIVITY_CONNECTED:
        return UI_COLOR_SUCCESS;
    case CONNECTIVITY_OFFLINE:
        return UI_COLOR_WARNING_TEXT;
    case CONNECTIVITY_SEARCHING:
    case CONNECTIVITY_NO_NETWORKS:
    default:
        return UI_COLOR_TEXT_SECONDARY;
    }
}

static void create_state_line(lv_obj_t *parent)
{
    char text[sizeof(UI_TEXT_WIFI_STATE_CONNECTED_FORMAT) + WIFI_SSID_SIZE];
    const bool failed = shown.list_failed;
    lv_obj_t *line = create_label(parent, failed ? UI_TEXT_WIFI_LIST_FAILED : state_text(&shown, text, sizeof(text)),
                                  failed ? UI_COLOR_ERROR : state_color(&shown));
    lv_obj_set_width(line, UI_CONTENT_WIDTH_PX);
    lv_label_set_long_mode(line, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(line, UI_SCREEN_MARGIN_PX, WIFI_STATE_Y_PX);
}

static void create_network_row(lv_obj_t *list, unsigned index)
{
    ui_styles_t *styles = ui_theme_styles();
    const bool connected = shown.state == CONNECTIVITY_CONNECTED && shown.connected_index == index;

    lv_obj_t *row = lv_button_create(list);
    lv_obj_add_style(row, &styles->button, LV_PART_MAIN);
    lv_obj_add_style(row, &styles->button_pressed, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_size(row, lv_pct(100), WIFI_ROW_HEIGHT_PX);
    lv_obj_set_style_pad_hor(row, WIFI_ROW_PAD_PX, LV_PART_MAIN);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(row, UI_ICON_TEXT_GAP_PX, LV_PART_MAIN);
    lv_obj_add_event_cb(row, on_row_tapped, LV_EVENT_CLICKED, (void *)(uintptr_t)index);

    if (connected) {
        create_label(row, LV_SYMBOL_OK, UI_COLOR_SUCCESS);
    }
    lv_obj_t *name = lv_label_create(row);
    lv_label_set_text(name, shown.saved[index]);
    lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
    lv_obj_set_flex_grow(name, 1);
    create_label(row, LV_SYMBOL_CLOSE, UI_COLOR_TEXT_SECONDARY);
}

static void create_network_list(lv_obj_t *parent)
{
    lv_obj_t *list = lv_obj_create(parent);
    network_list = list;
    lv_obj_remove_style_all(list);
    lv_obj_set_size(list, UI_CONTENT_WIDTH_PX, WIFI_LIST_HEIGHT_PX);
    lv_obj_set_pos(list, UI_SCREEN_MARGIN_PX, WIFI_LIST_Y_PX);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_gap(list, WIFI_ROW_GAP_PX, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);
    for (unsigned i = 0; i < shown.saved_count; ++i) {
        create_network_row(list, i);
    }
}

static void create_normal_content(lv_obj_t *parent)
{
    create_state_line(parent);
    create_network_list(parent);
    create_fixed_button(parent, UI_BUTTON_STANDARD, LV_SYMBOL_LEFT, UI_TEXT_BACK, UI_SCREEN_MARGIN_PX,
                        UI_BACK_WITH_ACTION_WIDTH_PX, handlers->on_back);
    create_fixed_button(parent, UI_BUTTON_PRIMARY, LV_SYMBOL_PLUS, UI_TEXT_WIFI_ADD_NETWORK,
                        UI_SCREEN_MARGIN_PX + UI_BACK_WITH_ACTION_WIDTH_PX + UI_GAP_PX, WIFI_ACTION_WIDTH_PX, on_add_requested);
}

static void create_qr(lv_obj_t *parent)
{
    char payload[WIFI_QR_TEXT_CAPACITY];
    const int length = snprintf(payload, sizeof(payload), UI_TEXT_WIFI_SETUP_QR_FORMAT, shown.setup_ssid,
                                shown.setup_password);
    lv_obj_t *qr = lv_qrcode_create(parent);
    lv_qrcode_set_size(qr, WIFI_QR_SIZE_PX);
    lv_qrcode_set_dark_color(qr, lv_color_hex(UI_COLOR_BACKGROUND));
    lv_qrcode_set_light_color(qr, lv_color_hex(UI_COLOR_TEXT));
    lv_obj_set_style_border_color(qr, lv_color_hex(UI_COLOR_TEXT), LV_PART_MAIN);
    lv_obj_set_style_border_width(qr, WIFI_QR_QUIET_ZONE_PX, LV_PART_MAIN);
    lv_obj_set_pos(qr, UI_SCREEN_MARGIN_PX, WIFI_SETUP_TOP_Y_PX);
    if (length > 0 && (size_t)length < sizeof(payload)) {
        lv_qrcode_update(qr, payload, (uint32_t)length);
    }
}

static void create_setup_field(lv_obj_t *column, const char *name, const char *value, ui_font_role_t font)
{
    lv_obj_t *field = ui_box_create(column, LV_FLEX_FLOW_COLUMN, 0);
    create_secondary_label(field, name);
    lv_obj_t *text = lv_label_create(field);
    lv_label_set_text(text, value);
    lv_obj_set_style_text_font(text, ui_theme_font(font), LV_PART_MAIN);
}

static void create_setup_content(lv_obj_t *parent)
{
    create_qr(parent);

    lv_obj_t *column = ui_box_create(parent, LV_FLEX_FLOW_COLUMN, UI_ROW_LABEL_GAP_PX);
    lv_obj_set_width(column, WIFI_SETUP_TEXT_WIDTH_PX);
    lv_obj_set_pos(column, WIFI_SETUP_TEXT_X_PX, WIFI_SETUP_TOP_Y_PX);
    create_setup_field(column, UI_TEXT_WIFI_SETUP_JOIN, shown.setup_ssid, UI_FONT_SECONDARY);
    create_setup_field(column, UI_TEXT_WIFI_SETUP_PASSWORD, shown.setup_password, UI_FONT_TITLE);
    create_setup_field(column, UI_TEXT_WIFI_SETUP_THEN_OPEN, SETUP_URL, UI_FONT_SECONDARY);

    if (shown.setup_password_rejected) {
        lv_obj_t *warning = create_label(parent, UI_TEXT_WIFI_SETUP_PASSWORD_REJECTED, UI_COLOR_ERROR);
        lv_obj_set_width(warning, UI_CONTENT_WIDTH_PX);
        lv_obj_set_pos(warning, UI_SCREEN_MARGIN_PX,
                       WIFI_SETUP_TOP_Y_PX + WIFI_QR_BOX_PX + UI_GAP_PX);
    }
    create_fixed_button(parent, UI_BUTTON_STANDARD, LV_SYMBOL_LEFT, UI_TEXT_BACK, UI_SCREEN_MARGIN_PX,
                        UI_BACK_WITH_ACTION_WIDTH_PX, handlers->on_back);
    create_fixed_button(parent, UI_BUTTON_STANDARD, LV_SYMBOL_CLOSE, UI_TEXT_WIFI_CANCEL_SETUP,
                        UI_SCREEN_MARGIN_PX + UI_BACK_WITH_ACTION_WIDTH_PX + UI_GAP_PX, WIFI_ACTION_WIDTH_PX, on_cancel_setup);
}

static lv_obj_t *create_dialog_panel(lv_obj_t *parent)
{
    lv_obj_t *scrim = lv_obj_create(parent);
    lv_obj_remove_style_all(scrim);
    lv_obj_set_size(scrim, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(scrim, lv_color_hex(UI_COLOR_BACKGROUND), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scrim, WIFI_DIALOG_SCRIM_OPA, LV_PART_MAIN);
    lv_obj_remove_flag(scrim, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *panel = lv_obj_create(scrim);
    lv_obj_remove_style_all(panel);
    lv_obj_set_size(panel, WIFI_DIALOG_WIDTH_PX, LV_SIZE_CONTENT);
    lv_obj_center(panel);
    lv_obj_set_style_bg_color(panel, lv_color_hex(UI_COLOR_SURFACE_RAISED), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(panel, UI_CORNER_RADIUS_PX, LV_PART_MAIN);
    lv_obj_set_style_pad_all(panel, WIFI_DIALOG_PAD_PX, LV_PART_MAIN);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_gap(panel, UI_GAP_PX, LV_PART_MAIN);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    return panel;
}

static void create_dialog_text(lv_obj_t *panel, const char *title, const char *body)
{
    lv_obj_t *title_label = lv_label_create(panel);
    lv_label_set_text(title_label, title);
    lv_obj_set_width(title_label, WIFI_DIALOG_INNER_WIDTH_PX);
    lv_label_set_long_mode(title_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(title_label, ui_theme_font(UI_FONT_TITLE), LV_PART_MAIN);
    lv_obj_t *body_label = create_secondary_label(panel, body);
    lv_obj_set_width(body_label, WIFI_DIALOG_INNER_WIDTH_PX);
    lv_label_set_long_mode(body_label, LV_LABEL_LONG_WRAP);
}

static void create_dialog_buttons(lv_obj_t *panel, const char *action, lv_event_cb_t on_action)
{
    lv_obj_t *buttons = ui_box_create(panel, LV_FLEX_FLOW_ROW, UI_GAP_PX);
    lv_obj_t *cancel = create_button(buttons, UI_BUTTON_STANDARD, NULL, UI_TEXT_WIFI_CANCEL);
    lv_obj_set_size(cancel, WIFI_DIALOG_BUTTON_WIDTH_PX, UI_BUTTON_HEIGHT_PX);
    lv_obj_add_event_cb(cancel, on_dialog_cancel, LV_EVENT_CLICKED, NULL);
    lv_obj_t *confirm = create_button(buttons, UI_BUTTON_PRIMARY, NULL, action);
    lv_obj_set_size(confirm, WIFI_DIALOG_BUTTON_WIDTH_PX, UI_BUTTON_HEIGHT_PX);
    lv_obj_add_event_cb(confirm, on_action, LV_EVENT_CLICKED, NULL);
}

static void create_dialog(lv_obj_t *parent)
{
    lv_obj_t *panel = create_dialog_panel(parent);
    dialog = lv_obj_get_parent(panel);
    char title[sizeof(UI_TEXT_WIFI_FORGET_TITLE_FORMAT) + WIFI_SSID_SIZE];
    if (dialog_kind == UI_WIFI_DIALOG_FORGET) {
        snprintf(title, sizeof(title), UI_TEXT_WIFI_FORGET_TITLE_FORMAT, dialog_ssid);
        create_dialog_text(panel, title, UI_TEXT_WIFI_FORGET_BODY);
        create_dialog_buttons(panel, UI_TEXT_WIFI_FORGET, on_forget_confirmed);
        return;
    }
    char body[sizeof(UI_TEXT_WIFI_ADD_BODY_FORMAT) + WIFI_SSID_SIZE];
    snprintf(body, sizeof(body), UI_TEXT_WIFI_ADD_BODY_FORMAT, shown.saved[shown.connected_index]);
    create_dialog_text(panel, UI_TEXT_WIFI_ADD_TITLE, body);
    create_dialog_buttons(panel, UI_TEXT_WIFI_CONTINUE, on_add_confirmed);
}

static bool forget_target_exists(void)
{
    for (unsigned i = 0; i < shown.saved_count; ++i) {
        if (strcmp(shown.saved[i], dialog_ssid) == 0) {
            return true;
        }
    }
    return false;
}

static bool dialog_still_valid(void)
{
    if (shown.setup_active) {
        return false;
    }
    return dialog_kind == UI_WIFI_DIALOG_FORGET ? forget_target_exists() : shown.state == CONNECTIVITY_CONNECTED;
}

/* The list keeps its scroll position across refreshes. */
static void rebuild_content(void)
{
    const int32_t scroll_y = network_list != NULL ? lv_obj_get_scroll_y(network_list) : 0;
    network_list = NULL;
    lv_obj_clean(content);
    if (shown.setup_active) {
        create_setup_content(content);
        return;
    }
    create_normal_content(content);
    lv_obj_update_layout(network_list);
    lv_obj_scroll_to_y(network_list, scroll_y, LV_ANIM_OFF);
}

/* Keeps the view usable if its indexes are out of range: a connected index outside the list means no link shown. */
static void accept_view(const connectivity_view_t *view)
{
    shown = *view;
    if (shown.saved_count > WIFI_LIST_CAPACITY) {
        shown.saved_count = WIFI_LIST_CAPACITY;
    }
    if (shown.state == CONNECTIVITY_CONNECTED && shown.connected_index >= shown.saved_count) {
        shown.state = CONNECTIVITY_OFFLINE;
    }
}

static void forget_screen(lv_event_t *event)
{
    /* A screen replaced after its successor was created must not clear the successor's state. */
    if (lv_event_get_target(event) != screen_object) {
        return;
    }
    const ui_wifi_handlers_t *closing = handlers;
    handlers = NULL;
    screen_object = NULL;
    content = NULL;
    dialog = NULL;
    network_list = NULL;
    closing->on_closed();
}

lv_obj_t *ui_wifi_screen_create(const connectivity_view_t *view, const ui_wifi_handlers_t *on_events)
{
    handlers = on_events;
    accept_view(view);
    dialog = NULL;

    screen_object = ui_screen_create();
    ui_status_bar_create(screen_object, UI_TEXT_WIFI);
    content = lv_obj_create(screen_object);
    lv_obj_remove_style_all(content);
    lv_obj_set_size(content, lv_pct(100), lv_pct(100));
    lv_obj_remove_flag(content, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(screen_object, forget_screen, LV_EVENT_DELETE, NULL);
    rebuild_content();
    return screen_object;
}

void ui_wifi_show(const connectivity_view_t *view)
{
    if (handlers == NULL) {
        return;
    }
    accept_view(view);
    rebuild_content();
    if (dialog != NULL && !dialog_still_valid()) {
        close_dialog();
    }
}

void ui_wifi_open_dialog(ui_wifi_dialog_t kind, const char *ssid)
{
    if (handlers == NULL) {
        return;
    }
    close_dialog();
    dialog_kind = kind;
    snprintf(dialog_ssid, sizeof(dialog_ssid), "%s", ssid != NULL ? ssid : "");
    create_dialog(screen_object);
}
