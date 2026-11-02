#include "ui_notifications.h"

#include <string.h>

#include "ui_notifications_text.h"
#include "ui_status_bar.h"
#include "ui_theme.h"
#include "ui_widgets.h"

#define CARD_TOP_PX (UI_STATUS_BAR_HEIGHT_PX + UI_GAP_PX)
#define CARD_HEIGHT_PX (BOARD_DISPLAY_HEIGHT_PX - CARD_TOP_PX - UI_SCREEN_MARGIN_PX)
#define MESSAGE_HEIGHT_PX 160
#define NOTICE_HEIGHT_PX 44

static lv_obj_t *overlay;
static lv_obj_t *backdrop;
static lv_obj_t *message;
static lv_obj_t *notice;
static lv_obj_t *actions;
static lv_obj_t *close_button;
static lv_obj_t *snooze_button;
static lv_obj_t *presets;
static notification_view_t shown;
static ui_notifications_handlers_t callbacks;
static bool has_callbacks;
static bool selecting_preset;

static const char *notice_text(notification_view_notice_t value)
{
    switch (value) {
    case NOTIFICATION_VIEW_STORAGE_ERROR: return UI_TEXT_NOTIFICATION_STORAGE_ERROR;
    case NOTIFICATION_VIEW_SNOOZE_REJECTED: return UI_TEXT_NOTIFICATION_SNOOZE_REJECTED;
    case NOTIFICATION_VIEW_SNOOZE_NOT_FOUND: return UI_TEXT_NOTIFICATION_SNOOZE_NOT_FOUND;
    case NOTIFICATION_VIEW_SNOOZE_SCHEDULE_CHANGED: return UI_TEXT_NOTIFICATION_SNOOZE_SCHEDULE_CHANGED;
    case NOTIFICATION_VIEW_SNOOZE_NOT_SENT: return UI_TEXT_NOTIFICATION_SNOOZE_NOT_SENT;
    case NOTIFICATION_VIEW_SNOOZE_UNCONFIRMED: return UI_TEXT_NOTIFICATION_SNOOZE_UNCONFIRMED;
    case NOTIFICATION_VIEW_SNOOZE_CONFIRMED: return UI_TEXT_NOTIFICATION_SNOOZE_CONFIRMED;
    case NOTIFICATION_VIEW_OK: return "";
    }
    return "";
}

static void on_close(lv_event_t *event)
{
    (void)event;
    if (shown.count == 0U && shown.notice == NOTIFICATION_VIEW_SNOOZE_CONFIRMED) {
        if (has_callbacks && callbacks.close != NULL) callbacks.close(shown.generation, "");
        ui_notifications_hide();
        return;
    }
    if (has_callbacks && callbacks.close != NULL && !shown.action_busy &&
        shown.notice != NOTIFICATION_VIEW_STORAGE_ERROR)
        callbacks.close(shown.generation, shown.card.event_id);
}

static void on_snooze(lv_event_t *event)
{
    (void)event;
    if (shown.action_busy || shown.notice == NOTIFICATION_VIEW_STORAGE_ERROR ||
        shown.snooze_close_pending) return;
    selecting_preset = true;
    lv_obj_add_flag(message, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(notice, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(actions, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(presets, LV_OBJ_FLAG_HIDDEN);
}

static void on_cancel(lv_event_t *event)
{
    (void)event;
    selecting_preset = false;
    lv_obj_remove_flag(message, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(notice, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(presets, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(actions, LV_OBJ_FLAG_HIDDEN);
}

static void on_preset(lv_event_t *event)
{
    const unsigned preset = (unsigned)(uintptr_t)lv_event_get_user_data(event);
    selecting_preset = false;
    lv_obj_remove_flag(message, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(notice, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(presets, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(actions, LV_OBJ_FLAG_HIDDEN);
    if (has_callbacks && callbacks.snooze != NULL && !shown.action_busy &&
        callbacks.snooze(shown.generation, shown.card.event_id, preset)) {
        shown.action_busy = true;
        lv_obj_add_state(actions, LV_STATE_DISABLED);
    } else {
        lv_label_set_text_static(notice, UI_TEXT_NOTIFICATION_SNOOZE_NOT_SENT);
        lv_obj_remove_flag(notice, LV_OBJ_FLAG_HIDDEN);
    }
}

static lv_obj_t *button(lv_obj_t *parent, const char *text, lv_event_cb_t on_click, uintptr_t data)
{
    lv_obj_t *result = ui_button_create(parent, UI_BUTTON_STANDARD, NULL, text);
    lv_obj_set_height(result, UI_BUTTON_HEIGHT_PX);
    lv_obj_set_flex_grow(result, 1);
    lv_obj_add_event_cb(result, on_click, LV_EVENT_CLICKED, (void *)data);
    return result;
}

static lv_obj_t *row(lv_obj_t *parent)
{
    lv_obj_t *result = ui_box_create(parent, LV_FLEX_FLOW_ROW, UI_GAP_PX);
    lv_obj_set_width(result, lv_pct(100));
    return result;
}

static void create_overlay(void)
{
    backdrop = lv_obj_create(lv_layer_top());
    lv_obj_set_size(backdrop, BOARD_DISPLAY_WIDTH_PX, BOARD_DISPLAY_HEIGHT_PX - UI_STATUS_BAR_HEIGHT_PX);
    lv_obj_align(backdrop, LV_ALIGN_TOP_MID, 0, UI_STATUS_BAR_HEIGHT_PX);
    lv_obj_set_style_bg_color(backdrop, lv_color_hex(UI_COLOR_BACKGROUND), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(backdrop, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(backdrop, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(backdrop, 0, LV_PART_MAIN);
    lv_obj_add_flag(backdrop, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(backdrop, LV_OBJ_FLAG_SCROLLABLE);
    overlay = lv_obj_create(backdrop);
    lv_obj_set_size(overlay, UI_CONTENT_WIDTH_PX, CARD_HEIGHT_PX);
    lv_obj_align(overlay, LV_ALIGN_TOP_MID, 0, UI_GAP_PX);
    lv_obj_set_style_bg_color(overlay, lv_color_hex(UI_COLOR_SURFACE_RAISED), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_text_color(overlay, lv_color_hex(UI_COLOR_TEXT), LV_PART_MAIN);
    lv_obj_set_style_radius(overlay, UI_CORNER_RADIUS_PX, LV_PART_MAIN);
    lv_obj_set_style_pad_all(overlay, UI_GAP_PX, LV_PART_MAIN);
    lv_obj_set_style_pad_row(overlay, UI_GAP_PX, LV_PART_MAIN);
    lv_obj_set_flex_flow(overlay, LV_FLEX_FLOW_COLUMN);
    lv_obj_remove_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(overlay);
    lv_label_set_text_static(title, UI_TEXT_NOTIFICATION_CARD_TITLE);
    lv_obj_set_style_text_font(title, ui_theme_font(UI_FONT_TITLE), LV_PART_MAIN);

    message = lv_label_create(overlay);
    lv_obj_set_width(message, lv_pct(100));
    lv_obj_set_height(message, MESSAGE_HEIGHT_PX);
    lv_label_set_long_mode(message, LV_LABEL_LONG_WRAP);
    lv_obj_add_flag(message, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(message, LV_SCROLLBAR_MODE_ACTIVE);

    notice = lv_label_create(overlay);
    lv_obj_set_width(notice, lv_pct(100));
    lv_obj_set_height(notice, NOTICE_HEIGHT_PX);
    lv_label_set_long_mode(notice, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_color(notice, lv_color_hex(UI_COLOR_WARNING_TEXT), LV_PART_MAIN);

    actions = row(overlay);
    close_button = button(actions, UI_TEXT_NOTIFICATION_CLOSE, on_close, 0);
    snooze_button = button(actions, UI_TEXT_NOTIFICATION_SNOOZE, on_snooze, 0);

    presets = ui_box_create(overlay, LV_FLEX_FLOW_COLUMN, UI_GAP_PX);
    lv_obj_t *first_row = row(presets);
    button(first_row, UI_TEXT_NOTIFICATION_SNOOZE_FIVE, on_preset, 1U);
    button(first_row, UI_TEXT_NOTIFICATION_SNOOZE_HOUR, on_preset, 2U);
    lv_obj_t *second_row = row(presets);
    button(second_row, UI_TEXT_NOTIFICATION_SNOOZE_TOMORROW, on_preset, 3U);
    button(second_row, UI_TEXT_NOTIFICATION_SNOOZE_CANCEL, on_cancel, 0U);
    lv_obj_add_flag(presets, LV_OBJ_FLAG_HIDDEN);
}

void ui_notifications_hide(void)
{
    if (backdrop != NULL) lv_obj_delete(backdrop);
    backdrop = overlay = message = notice = actions = presets = close_button = snooze_button = NULL;
    callbacks = (ui_notifications_handlers_t){0};
    has_callbacks = false;
    ui_status_bar_set_title_override(NULL);
    selecting_preset = false;
    memset(&shown, 0, sizeof(shown));
}

void ui_notifications_show(const notification_view_t *view, const ui_notifications_handlers_t *handlers)
{
    if (view == NULL || (view->count == 0U && view->notice != NOTIFICATION_VIEW_STORAGE_ERROR &&
                         view->notice != NOTIFICATION_VIEW_SNOOZE_CONFIRMED)) {
        ui_notifications_hide();
        return;
    }
    const bool changed = overlay == NULL || strcmp(shown.card.event_id, view->card.event_id) != 0 ||
                         (view->count == 0U && shown.notice != view->notice);
    if (changed) {
        ui_notifications_hide();
        create_overlay();
        if (view->count == 0U && view->notice == NOTIFICATION_VIEW_SNOOZE_CONFIRMED)
            lv_label_set_text_fmt(message, UI_TEXT_NOTIFICATION_SNOOZE_CONFIRMED, view->snoozed_at_text);
        else lv_label_set_text(message, view->count == 0U ? UI_TEXT_NOTIFICATION_STORAGE_ERROR : view->card.message);
    }
    ui_status_bar_set_title_override(UI_TEXT_NOTIFICATION_TITLE);
    shown = *view;
    callbacks = handlers != NULL ? *handlers : (ui_notifications_handlers_t){0};
    has_callbacks = handlers != NULL;
    if (view->count == 0U) {
        lv_obj_add_flag(notice, LV_OBJ_FLAG_HIDDEN);
        if (view->notice == NOTIFICATION_VIEW_SNOOZE_CONFIRMED) {
            lv_obj_remove_flag(actions, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(snooze_button, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text_static(lv_obj_get_child(close_button, -1), UI_TEXT_NOTIFICATION_OK);
        } else lv_obj_add_flag(actions, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(presets, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_remove_flag(snooze_button, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text_static(lv_obj_get_child(close_button, -1),
                             view->count > 1U ? UI_TEXT_NOTIFICATION_NEXT : UI_TEXT_NOTIFICATION_CLOSE);
    if (view->notice == NOTIFICATION_VIEW_SNOOZE_CONFIRMED) {
        lv_label_set_text_fmt(notice, view->snooze_close_pending ? UI_TEXT_NOTIFICATION_SNOOZE_CLOSE_PENDING
                                                                 : UI_TEXT_NOTIFICATION_SNOOZE_CONFIRMED,
                              view->snoozed_at_text);
    } else {
        lv_label_set_text_static(notice, notice_text(view->notice));
    }
    if (view->action_busy || view->snooze_close_pending) selecting_preset = false;
    if (selecting_preset) {
        lv_obj_add_flag(message, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(notice, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(actions, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(presets, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_remove_flag(message, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(notice, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(actions, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(presets, LV_OBJ_FLAG_HIDDEN);
    }
    if (view->action_busy || view->notice == NOTIFICATION_VIEW_STORAGE_ERROR)
        lv_obj_add_state(actions, LV_STATE_DISABLED);
    else lv_obj_remove_state(actions, LV_STATE_DISABLED);
    if (callbacks.snooze == NULL || view->action_busy || view->notice == NOTIFICATION_VIEW_STORAGE_ERROR ||
        view->snooze_close_pending)
        lv_obj_add_state(snooze_button, LV_STATE_DISABLED);
    else lv_obj_remove_state(snooze_button, LV_STATE_DISABLED);
}
