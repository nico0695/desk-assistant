#include "ui_recording.h"

#include <stdbool.h>
#include <stddef.h>

#include "ui_recording_text.h"
#include "ui_status_bar.h"
#include "ui_text.h"
#include "ui_theme.h"
#include "ui_widgets.h"

#define RECORDING_CONTENT_Y_PX (UI_STATUS_BAR_HEIGHT_PX + 2 * UI_GAP_PX)
#define RECORDING_BOTTOM_Y_PX (BOARD_DISPLAY_HEIGHT_PX - UI_SCREEN_MARGIN_PX - UI_BUTTON_HEIGHT_PX)
#define RECORDING_HALF_WIDTH_PX ((UI_CONTENT_WIDTH_PX - UI_GAP_PX) / 2)
#define SECONDS_PER_MINUTE 60U
#define MS_PER_SECOND 1000U

typedef struct {
    const char *title;
    const char *message;
} presentation_t;

/* State of the open panel; cleared when it is deleted. */
static const ui_recording_handlers_t *handlers;
static uint64_t shown_generation;
static lv_obj_t *screen_object;
static lv_obj_t *dot;
static lv_obj_t *title;
static lv_obj_t *duration;
static lv_obj_t *message;
static lv_obj_t *cancel;
static lv_obj_t *send;
static lv_obj_t *close_button;

static presentation_t failure_presentation(recording_failure_t failure)
{
    switch (failure) {
    case RECORDING_FAILURE_START:
        return (presentation_t){UI_TEXT_RECORDING_TITLE_START_FAILED, UI_TEXT_RECORDING_MESSAGE_START_FAILED};
    case RECORDING_FAILURE_TOO_SHORT:
        return (presentation_t){UI_TEXT_RECORDING_TITLE_TOO_SHORT, UI_TEXT_RECORDING_MESSAGE_TOO_SHORT};
    case RECORDING_FAILURE_INTERRUPTED:
        return (presentation_t){UI_TEXT_RECORDING_TITLE_INTERRUPTED, UI_TEXT_RECORDING_MESSAGE_INTERRUPTED};
    case RECORDING_FAILURE_STORAGE:
        return (presentation_t){UI_TEXT_RECORDING_TITLE_STORAGE_FAILED, UI_TEXT_RECORDING_MESSAGE_STORAGE_FAILED};
    case RECORDING_FAILURE_CLEANUP:
    case RECORDING_FAILURE_NONE:
    default:
        return (presentation_t){UI_TEXT_RECORDING_TITLE_CLEANUP_FAILED, UI_TEXT_RECORDING_MESSAGE_CLEANUP_FAILED};
    }
}

static presentation_t presentation(const recording_view_t *view)
{
    switch (view->phase) {
    case RECORDING_RECORDING:
        return (presentation_t){UI_TEXT_RECORDING_TITLE_RECORDING, UI_TEXT_RECORDING_MESSAGE_RECORDING};
    case RECORDING_FINALIZING:
        return (presentation_t){UI_TEXT_RECORDING_TITLE_FINALIZING, UI_TEXT_RECORDING_MESSAGE_FINALIZING};
    case RECORDING_READY_TO_SEND:
        return (presentation_t){UI_TEXT_RECORDING_TITLE_READY, UI_TEXT_RECORDING_MESSAGE_READY};
    case RECORDING_FAILED:
        return failure_presentation(view->failure);
    case RECORDING_IDLE:
    case RECORDING_CANCELLING:
    default:
        return (presentation_t){UI_TEXT_RECORDING_TITLE_CANCELLING, UI_TEXT_RECORDING_MESSAGE_CANCELLING};
    }
}

static void set_visible(lv_obj_t *object, bool visible)
{
    if (visible) {
        lv_obj_remove_flag(object, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
    }
}

static void on_send(lv_event_t *event)
{
    (void)event;
    handlers->on_send(shown_generation);
}

static void on_cancel(lv_event_t *event)
{
    (void)event;
    handlers->on_cancel(shown_generation);
}

static void on_close(lv_event_t *event)
{
    (void)event;
    handlers->on_close(shown_generation);
}

static void forget_screen(lv_event_t *event)
{
    if (lv_event_get_target(event) != screen_object) {
        return;
    }
    handlers = NULL;
    screen_object = NULL;
}

static lv_obj_t *create_action(lv_obj_t *parent, ui_button_kind_t kind, const char *symbol, const char *text,
                               int32_t x, int32_t width, lv_event_cb_t on_click)
{
    lv_obj_t *button = ui_button_create(parent, kind, symbol, text);
    lv_obj_set_style_min_height(button, UI_BUTTON_HEIGHT_PX, LV_PART_MAIN);
    lv_obj_set_size(button, width, UI_BUTTON_HEIGHT_PX);
    lv_obj_set_pos(button, x, RECORDING_BOTTOM_Y_PX);
    lv_obj_add_event_cb(button, on_click, LV_EVENT_CLICKED, NULL);
    return button;
}

static void create_header(lv_obj_t *parent)
{
    lv_obj_t *header = ui_box_create(parent, LV_FLEX_FLOW_ROW, UI_ICON_TEXT_GAP_PX);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    dot = lv_obj_create(header);
    lv_obj_remove_style_all(dot);
    lv_obj_set_size(dot, UI_RECORD_DOT_SIZE_PX, UI_RECORD_DOT_SIZE_PX);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(dot, lv_color_hex(UI_COLOR_RECORD), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, LV_PART_MAIN);
    title = lv_label_create(header);
    lv_obj_set_style_text_font(title, ui_theme_font(UI_FONT_TITLE), LV_PART_MAIN);
}

lv_obj_t *ui_recording_screen_create(const recording_view_t *view, const ui_recording_handlers_t *on_events)
{
    handlers = on_events;
    screen_object = ui_screen_create();
    ui_status_bar_create(screen_object, UI_TEXT_RECORD);
    lv_obj_add_event_cb(screen_object, forget_screen, LV_EVENT_DELETE, NULL);

    lv_obj_t *content = ui_box_create(screen_object, LV_FLEX_FLOW_COLUMN, UI_GAP_PX);
    lv_obj_set_width(content, UI_CONTENT_WIDTH_PX);
    lv_obj_set_pos(content, UI_SCREEN_MARGIN_PX, RECORDING_CONTENT_Y_PX);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    create_header(content);
    duration = lv_label_create(content);
    lv_obj_set_style_text_font(duration, ui_theme_font(UI_FONT_TITLE), LV_PART_MAIN);
    message = lv_label_create(content);
    lv_obj_add_style(message, &ui_theme_styles()->text_secondary, LV_PART_MAIN);
    lv_obj_set_width(message, UI_CONTENT_WIDTH_PX);
    lv_obj_set_style_text_align(message, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_long_mode(message, LV_LABEL_LONG_WRAP);

    cancel = create_action(screen_object, UI_BUTTON_STANDARD, LV_SYMBOL_CLOSE, UI_TEXT_RECORDING_CANCEL,
                           UI_SCREEN_MARGIN_PX, RECORDING_HALF_WIDTH_PX, on_cancel);
    send = create_action(screen_object, UI_BUTTON_PRIMARY, LV_SYMBOL_UPLOAD, UI_TEXT_RECORDING_SEND,
                         UI_SCREEN_MARGIN_PX + RECORDING_HALF_WIDTH_PX + UI_GAP_PX, RECORDING_HALF_WIDTH_PX, on_send);
    close_button = create_action(screen_object, UI_BUTTON_STANDARD, NULL, UI_TEXT_RECORDING_CLOSE,
                                 UI_SCREEN_MARGIN_PX, UI_CONTENT_WIDTH_PX, on_close);
    ui_recording_show(view);
    return screen_object;
}

void ui_recording_show(const recording_view_t *view)
{
    if (handlers == NULL) {
        return;
    }
    shown_generation = view->generation;
    const presentation_t text = presentation(view);
    lv_label_set_text_static(title, text.title);
    lv_label_set_text_static(message, text.message);

    const bool capturing = view->phase == RECORDING_RECORDING;
    const bool ready = view->phase == RECORDING_READY_TO_SEND;
    const bool failed = view->phase == RECORDING_FAILED;
    set_visible(dot, capturing);
    set_visible(duration, capturing || view->phase == RECORDING_FINALIZING || ready);
    const unsigned elapsed_s = view->duration_ms / MS_PER_SECOND;
    const unsigned max_s = RECORDING_MAX_MS / MS_PER_SECOND;
    lv_label_set_text_fmt(duration, UI_TEXT_RECORDING_DURATION_FORMAT, elapsed_s / SECONDS_PER_MINUTE,
                          elapsed_s % SECONDS_PER_MINUTE, max_s / SECONDS_PER_MINUTE, max_s % SECONDS_PER_MINUTE);

    set_visible(cancel, capturing || ready);
    set_visible(send, capturing || ready);
    /* Sending arrives in stage 5b: until then a ready clip can only be cancelled. */
    if (ready) {
        lv_obj_add_state(send, LV_STATE_DISABLED);
    } else {
        lv_obj_remove_state(send, LV_STATE_DISABLED);
    }
    set_visible(close_button, failed);
}
