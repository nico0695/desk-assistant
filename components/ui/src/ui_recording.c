#include "ui_recording.h"

#include <stdbool.h>
#include <stddef.h>

#include "ui_recording_text.h"
#include "ui_status_bar.h"
#include "ui_text.h"
#include "ui_text_support.h"
#include "ui_theme.h"
#include "ui_widgets.h"

#define RECORDING_HEADER_Y_PX (UI_STATUS_BAR_HEIGHT_PX + 2 * UI_GAP_PX)
#define RECORDING_HEADER_HEIGHT_PX 32
#define RECORDING_BODY_Y_PX (RECORDING_HEADER_Y_PX + RECORDING_HEADER_HEIGHT_PX + UI_GAP_PX)
#define RECORDING_BOTTOM_Y_PX (BOARD_DISPLAY_HEIGHT_PX - UI_SCREEN_MARGIN_PX - UI_BUTTON_HEIGHT_PX)
#define RECORDING_BODY_HEIGHT_PX (RECORDING_BOTTOM_Y_PX - UI_GAP_PX - RECORDING_BODY_Y_PX)
#define SECONDS_PER_MINUTE 60U
#define MS_PER_SECOND 1000U

typedef struct {
    const char *title;
    /* NULL when the bridge message takes its place. */
    const char *message;
} presentation_t;

/* State of the open panel; cleared when it is deleted. */
static const ui_recording_handlers_t *handlers;
static recording_view_t shown_view;
static uint64_t message_generation;
static bool message_supported;
static bool confirming_discard;
static lv_obj_t *screen_object;
static lv_obj_t *dot;
static lv_obj_t *title;
static lv_obj_t *duration;
static lv_obj_t *message;
static lv_obj_t *reply;
static lv_obj_t *notes;
static lv_obj_t *cancel;
static lv_obj_t *send;
static lv_obj_t *check;
static lv_obj_t *discard;
static lv_obj_t *keep;
static lv_obj_t *confirm;
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
    case RECORDING_FAILURE_NOT_SENT:
        return (presentation_t){UI_TEXT_RECORDING_TITLE_NOT_SENT, UI_TEXT_RECORDING_MESSAGE_NOT_SENT};
    case RECORDING_FAILURE_REMOTE:
        return (presentation_t){UI_TEXT_RECORDING_TITLE_REMOTE_FAILED, NULL};
    case RECORDING_FAILURE_RECORD:
        return (presentation_t){UI_TEXT_RECORDING_TITLE_RECORD_FAILED, UI_TEXT_RECORDING_MESSAGE_RECORD_FAILED};
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
        return (presentation_t){UI_TEXT_RECORDING_TITLE_READY, view->notice == RECORDING_NOTICE_NOT_CONNECTED
                                                                   ? UI_TEXT_RECORDING_MESSAGE_NOT_CONNECTED
                                                                   : UI_TEXT_RECORDING_MESSAGE_READY};
    case RECORDING_UPLOADING:
        return (presentation_t){UI_TEXT_RECORDING_TITLE_UPLOADING, UI_TEXT_RECORDING_MESSAGE_UPLOADING};
    case RECORDING_PROCESSING:
        return (presentation_t){UI_TEXT_RECORDING_TITLE_PROCESSING, UI_TEXT_RECORDING_MESSAGE_PROCESSING};
    case RECORDING_COMPLETED:
        return (presentation_t){UI_TEXT_RECORDING_TITLE_COMPLETED, NULL};
    case RECORDING_UNKNOWN:
        return (presentation_t){UI_TEXT_RECORDING_TITLE_UNKNOWN, NULL};
    case RECORDING_UNCONFIRMED:
        if (view->failure == RECORDING_FAILURE_RECORD) {
            return failure_presentation(view->failure);
        }
        return (presentation_t){UI_TEXT_RECORDING_TITLE_UNCONFIRMED, view->can_discard
                                                                         ? UI_TEXT_RECORDING_MESSAGE_UNCONFIRMED
                                                                         : UI_TEXT_RECORDING_MESSAGE_UNCONFIRMED_KEPT};
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
    handlers->on_send(shown_view.generation);
}

static void on_cancel(lv_event_t *event)
{
    (void)event;
    handlers->on_cancel(shown_view.generation);
}

static void on_close(lv_event_t *event)
{
    (void)event;
    handlers->on_close(shown_view.generation);
}

static void on_check(lv_event_t *event)
{
    (void)event;
    handlers->on_check(shown_view.generation);
}

static void on_discard(lv_event_t *event)
{
    (void)event;
    confirming_discard = true;
    const recording_view_t view = shown_view;
    ui_recording_show(&view);
}

static void on_keep(lv_event_t *event)
{
    (void)event;
    confirming_discard = false;
    const recording_view_t view = shown_view;
    ui_recording_show(&view);
}

static void on_confirm(lv_event_t *event)
{
    (void)event;
    confirming_discard = false;
    handlers->on_discard(shown_view.generation);
}

static void forget_screen(lv_event_t *event)
{
    if (lv_event_get_target(event) != screen_object) {
        return;
    }
    handlers = NULL;
    screen_object = NULL;
}

static lv_obj_t *create_action(lv_obj_t *row, ui_button_kind_t kind, const char *text, lv_event_cb_t on_click)
{
    lv_obj_t *button = ui_button_create(row, kind, NULL, text);
    lv_obj_set_style_min_height(button, UI_BUTTON_HEIGHT_PX, LV_PART_MAIN);
    lv_obj_set_height(button, UI_BUTTON_HEIGHT_PX);
    lv_obj_set_flex_grow(button, 1);
    lv_obj_add_event_cb(button, on_click, LV_EVENT_CLICKED, NULL);
    return button;
}

static void create_header(lv_obj_t *parent)
{
    lv_obj_t *header = ui_box_create(parent, LV_FLEX_FLOW_ROW, UI_ICON_TEXT_GAP_PX);
    lv_obj_set_width(header, UI_CONTENT_WIDTH_PX);
    lv_obj_set_pos(header, UI_SCREEN_MARGIN_PX, RECORDING_HEADER_Y_PX);
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

static lv_obj_t *create_wrapped_label(lv_obj_t *parent, lv_text_align_t align)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_width(label, lv_pct(100));
    lv_obj_set_style_text_align(label, align, LV_PART_MAIN);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    return label;
}

/* Scrolls by swipe with a visible bar; the actions stay fixed below it. */
static void create_body(lv_obj_t *parent)
{
    lv_obj_t *body = ui_box_create(parent, LV_FLEX_FLOW_COLUMN, UI_GAP_PX);
    lv_obj_set_size(body, UI_CONTENT_WIDTH_PX, RECORDING_BODY_HEIGHT_PX);
    lv_obj_set_pos(body, UI_SCREEN_MARGIN_PX, RECORDING_BODY_Y_PX);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(body, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(body, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_width(body, UI_SCROLLBAR_WIDTH_PX, LV_PART_SCROLLBAR);
    lv_obj_set_style_radius(body, UI_SCROLLBAR_WIDTH_PX / 2, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_color(body, lv_color_hex(UI_COLOR_CONTROL_PRESSED), LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(body, LV_OPA_COVER, LV_PART_SCROLLBAR);
    lv_obj_set_style_pad_right(body, 2 * UI_SCROLLBAR_WIDTH_PX, LV_PART_MAIN);

    duration = lv_label_create(body);
    lv_obj_set_style_text_font(duration, ui_theme_font(UI_FONT_TITLE), LV_PART_MAIN);
    message = create_wrapped_label(body, LV_TEXT_ALIGN_CENTER);
    lv_obj_add_style(message, &ui_theme_styles()->text_secondary, LV_PART_MAIN);
    reply = create_wrapped_label(body, LV_TEXT_ALIGN_LEFT);
    lv_obj_set_style_text_font(reply, ui_theme_font(UI_FONT_SECONDARY), LV_PART_MAIN);
    notes = create_wrapped_label(body, LV_TEXT_ALIGN_LEFT);
    lv_obj_set_style_text_font(notes, ui_theme_font(UI_FONT_SECONDARY), LV_PART_MAIN);
    lv_obj_set_style_text_color(notes, lv_color_hex(UI_COLOR_WARNING_TEXT), LV_PART_MAIN);
}

static void create_actions(lv_obj_t *parent)
{
    lv_obj_t *row = ui_box_create(parent, LV_FLEX_FLOW_ROW, UI_GAP_PX);
    lv_obj_set_width(row, UI_CONTENT_WIDTH_PX);
    lv_obj_set_pos(row, UI_SCREEN_MARGIN_PX, RECORDING_BOTTOM_Y_PX);
    cancel = create_action(row, UI_BUTTON_STANDARD, UI_TEXT_RECORDING_CANCEL, on_cancel);
    send = create_action(row, UI_BUTTON_PRIMARY, UI_TEXT_RECORDING_SEND, on_send);
    discard = create_action(row, UI_BUTTON_STANDARD, UI_TEXT_RECORDING_DISCARD, on_discard);
    keep = create_action(row, UI_BUTTON_STANDARD, UI_TEXT_RECORDING_KEEP, on_keep);
    confirm = create_action(row, UI_BUTTON_PRIMARY, UI_TEXT_RECORDING_DISCARD, on_confirm);
    close_button = create_action(row, UI_BUTTON_STANDARD, UI_TEXT_RECORDING_CLOSE, on_close);
    check = create_action(row, UI_BUTTON_PRIMARY, UI_TEXT_RECORDING_CHECK, on_check);
}

lv_obj_t *ui_recording_screen_create(const recording_view_t *view, const ui_recording_handlers_t *on_events)
{
    handlers = on_events;
    message_generation = 0;
    confirming_discard = false;
    screen_object = ui_screen_create();
    ui_status_bar_create(screen_object, UI_TEXT_RECORD);
    lv_obj_add_event_cb(screen_object, forget_screen, LV_EVENT_DELETE, NULL);
    create_header(screen_object);
    create_body(screen_object);
    create_actions(screen_object);
    ui_recording_show(view);
    return screen_object;
}

static void copy_reply(const char *text, void *unused)
{
    (void)unused;
    lv_label_set_text(reply, text);
    message_supported = ui_text_support_covers(ui_theme_font(UI_FONT_SECONDARY), text);
}

/* Copies the bridge message once per generation; the owner keeps it until the attempt ends. */
static void show_reply(const recording_view_t *view)
{
    if (view->has_message && message_generation != view->generation) {
        message_supported = true;
        if (!handlers->read_message(view->generation, copy_reply, NULL)) {
            lv_label_set_text_static(reply, "");
        }
        message_generation = view->generation;
    } else if (!view->has_message && message_generation != 0U) {
        lv_label_set_text_static(reply, "");
        message_generation = 0;
    }
    const bool shown = view->has_message && !confirming_discard;
    const bool omitted = view->content_omitted;
    set_visible(reply, shown);
    set_visible(notes, shown && (omitted || !message_supported));
    lv_label_set_text_static(notes, omitted && !message_supported ? UI_TEXT_RECORDING_NOTE_OMITTED "\n"
                                                                    UI_TEXT_RECORDING_NOTE_UNSUPPORTED
                                    : omitted ? UI_TEXT_RECORDING_NOTE_OMITTED
                                              : UI_TEXT_RECORDING_NOTE_UNSUPPORTED);
}

static void show_duration(const recording_view_t *view)
{
    const bool visible = view->phase == RECORDING_RECORDING || view->phase == RECORDING_FINALIZING ||
                         view->phase == RECORDING_READY_TO_SEND;
    set_visible(duration, visible && !confirming_discard);
    const unsigned elapsed_s = view->duration_ms / MS_PER_SECOND;
    const unsigned max_s = RECORDING_MAX_MS / MS_PER_SECOND;
    lv_label_set_text_fmt(duration, UI_TEXT_RECORDING_DURATION_FORMAT, elapsed_s / SECONDS_PER_MINUTE,
                          elapsed_s % SECONDS_PER_MINUTE, max_s / SECONDS_PER_MINUTE, max_s % SECONDS_PER_MINUTE);
}

static void show_actions(const recording_view_t *view)
{
    const bool capturing = view->phase == RECORDING_RECORDING;
    const bool ready = view->phase == RECORDING_READY_TO_SEND;
    const bool closable = view->phase != RECORDING_IDLE && view->phase != RECORDING_RECORDING &&
                         view->phase != RECORDING_FINALIZING && view->phase != RECORDING_READY_TO_SEND &&
                         view->phase != RECORDING_CANCELLING;
    set_visible(cancel, !confirming_discard && (capturing || ready));
    set_visible(send, !confirming_discard && (capturing || ready));
    set_visible(discard, !confirming_discard && view->can_discard);
    set_visible(close_button, !confirming_discard && closable);
    set_visible(check, !confirming_discard && view->can_check);
    set_visible(keep, confirming_discard);
    set_visible(confirm, confirming_discard);
}

void ui_recording_show(const recording_view_t *view)
{
    if (handlers == NULL) {
        return;
    }
    if (view->generation != shown_view.generation || !view->can_discard) {
        confirming_discard = false;
    }
    shown_view = *view;
    const presentation_t text = confirming_discard
                                    ? (presentation_t){UI_TEXT_RECORDING_TITLE_DISCARD, UI_TEXT_RECORDING_MESSAGE_DISCARD}
                                    : presentation(view);
    const char *status = view->notice == RECORDING_NOTICE_NOT_CLEARED && !confirming_discard
                             ? UI_TEXT_RECORDING_MESSAGE_NOT_CLEARED
                         : text.message != NULL ? text.message
                         : view->has_message    ? ""
                                                : UI_TEXT_RECORDING_MESSAGE_UNAVAILABLE;
    lv_label_set_text_static(title, text.title);
    lv_label_set_text_static(message, status);
    set_visible(message, status[0] != '\0');
    set_visible(dot, view->phase == RECORDING_RECORDING && !confirming_discard);
    show_duration(view);
    show_reply(view);
    show_actions(view);
}
