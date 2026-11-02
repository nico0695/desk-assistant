#include "ui_modules.h"

#include <stdio.h>
#include <string.h>

#include "bridge_time.h"
#include "ui_modules_text.h"
#include "ui_status_bar.h"
#include "ui_text.h"
#include "ui_text_support.h"
#include "ui_theme.h"
#include "ui_widgets.h"

#define CONTENT_Y_PX (UI_STATUS_BAR_HEIGHT_PX + UI_GAP_PX)
#define CONTROLS_Y_PX (BOARD_DISPLAY_HEIGHT_PX - UI_SCREEN_MARGIN_PX - UI_BUTTON_HEIGHT_PX)
#define CONTENT_HEIGHT_PX (CONTROLS_Y_PX - UI_GAP_PX - CONTENT_Y_PX)
#define ICON_BUTTON_WIDTH_PX 80
#define ROW_TEXT_LINES 2
#define ROW_PAD_PX 12
#define FACT_LINE_SPACE_PX 6
#define TITLE_SIZE 32
#define DATE_SIZE 48
#define ID_SIZE 24
/* Status line, three dates, sent line and the ID, each with its label. */
#define FACTS_SIZE 384

/* State of the open screen; cleared when it is deleted. */
static const ui_modules_handlers_t *handlers;
static modules_view_t shown;
static lv_obj_t *screen_object;
static lv_obj_t *content;
static lv_obj_t *back_button;
static lv_obj_t *previous_button;
static lv_obj_t *refresh_button;
static lv_obj_t *next_button;
static char title_text[TITLE_SIZE];
/* The IDs of the rows on screen, in order. */
static uint64_t row_ids[BRIDGE_RESOURCE_PAGE_SIZE];

static const char *module_title(bridge_resource_kind_t kind)
{
    switch (kind) {
    case BRIDGE_RESOURCE_TASKS: return UI_TEXT_TASKS;
    case BRIDGE_RESOURCE_NOTES: return UI_TEXT_NOTES;
    case BRIDGE_RESOURCE_ALERTS:
    default: return UI_TEXT_ALERTS;
    }
}

static const char *error_text(modules_error_t error)
{
    switch (error) {
    case MODULES_ERROR_BUSY_AUDIO: return UI_TEXT_MODULES_ERROR_BUSY_AUDIO;
    case MODULES_ERROR_BUSY_BRIDGE: return UI_TEXT_MODULES_ERROR_BUSY_BRIDGE;
    case MODULES_ERROR_NETWORK: return UI_TEXT_MODULES_ERROR_NETWORK;
    case MODULES_ERROR_UNAUTHORIZED: return UI_TEXT_MODULES_ERROR_UNAUTHORIZED;
    case MODULES_ERROR_DENIED: return UI_TEXT_MODULES_ERROR_DENIED;
    case MODULES_ERROR_UNAVAILABLE: return UI_TEXT_MODULES_ERROR_UNAVAILABLE;
    case MODULES_ERROR_REJECTED: return UI_TEXT_MODULES_ERROR_REJECTED;
    case MODULES_ERROR_TOO_LARGE: return UI_TEXT_MODULES_ERROR_TOO_LARGE;
    case MODULES_ERROR_NOT_FOUND: return UI_TEXT_MODULES_ERROR_NOT_FOUND;
    case MODULES_ERROR_NO_MEMORY: return UI_TEXT_MODULES_ERROR_NO_MEMORY;
    case MODULES_ERROR_INVALID:
    case MODULES_ERROR_NONE:
    default: return UI_TEXT_MODULES_ERROR_INVALID;
    }
}

static const char *task_status_text(bridge_task_status_t status)
{
    switch (status) {
    case BRIDGE_TASK_PENDING: return UI_TEXT_MODULES_STATUS_PENDING;
    case BRIDGE_TASK_IN_PROGRESS: return UI_TEXT_MODULES_STATUS_IN_PROGRESS;
    case BRIDGE_TASK_COMPLETED: return UI_TEXT_MODULES_STATUS_COMPLETED;
    case BRIDGE_TASK_CANCELED:
    default: return UI_TEXT_MODULES_STATUS_CANCELED;
    }
}

/* Local time of a bridge UTC timestamp, or "Not available" when there is none. */
static void format_date(const char *utc, char text[DATE_SIZE])
{
    if (utc[0] == '\0' || !bridge_time_format_local(utc, text, DATE_SIZE))
        snprintf(text, DATE_SIZE, "%s", UI_TEXT_MODULES_NOT_AVAILABLE);
}

static void set_visible(lv_obj_t *object, bool visible)
{
    if (visible) lv_obj_remove_flag(object, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
}

static void set_enabled(lv_obj_t *object, bool enabled)
{
    if (enabled) lv_obj_remove_state(object, LV_STATE_DISABLED);
    else lv_obj_add_state(object, LV_STATE_DISABLED);
}

static lv_obj_t *create_label(lv_obj_t *parent, ui_font_role_t font, lv_text_align_t align)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_width(label, lv_pct(100));
    lv_obj_set_style_text_font(label, ui_theme_font(font), LV_PART_MAIN);
    lv_obj_set_style_text_align(label, align, LV_PART_MAIN);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    return label;
}

static void on_back(lv_event_t *event)
{
    (void)event;
    if (handlers == NULL) return;
    if (shown.screen == MODULES_SCREEN_LIST) handlers->on_leave();
    else handlers->on_back(shown.ticket);
}

static void on_previous(lv_event_t *event)
{
    (void)event;
    if (handlers != NULL) handlers->on_previous(shown.ticket);
}

static void on_next(lv_event_t *event)
{
    (void)event;
    if (handlers != NULL) handlers->on_next(shown.ticket);
}

static void on_refresh(lv_event_t *event)
{
    (void)event;
    if (handlers != NULL) handlers->on_refresh(shown.ticket);
}

static void on_row(lv_event_t *event)
{
    const size_t index = (size_t)(uintptr_t)lv_event_get_user_data(event);
    if (handlers != NULL && index < BRIDGE_RESOURCE_PAGE_SIZE) handlers->on_open(shown.ticket, row_ids[index]);
}

static void forget_screen(lv_event_t *event)
{
    if (lv_event_get_target(event) != screen_object) return;
    handlers = NULL;
    screen_object = NULL;
    content = NULL;
}

static lv_obj_t *create_icon_button(lv_obj_t *row, const char *symbol, lv_event_cb_t on_click)
{
    lv_obj_t *button = ui_button_create(row, UI_BUTTON_STANDARD, symbol, "");
    lv_obj_t *label = lv_obj_get_child(button, -1);
    lv_obj_delete(label);
    lv_obj_set_size(button, ICON_BUTTON_WIDTH_PX, UI_BUTTON_HEIGHT_PX);
    lv_obj_add_event_cb(button, on_click, LV_EVENT_CLICKED, NULL);
    return button;
}

static void create_controls(lv_obj_t *parent)
{
    lv_obj_t *row = ui_box_create(parent, LV_FLEX_FLOW_ROW, UI_GAP_PX);
    lv_obj_set_width(row, UI_CONTENT_WIDTH_PX);
    lv_obj_set_pos(row, UI_SCREEN_MARGIN_PX, CONTROLS_Y_PX);
    back_button = ui_button_create(row, UI_BUTTON_STANDARD, LV_SYMBOL_LEFT, UI_TEXT_BACK);
    lv_obj_set_height(back_button, UI_BUTTON_HEIGHT_PX);
    lv_obj_set_flex_grow(back_button, 1);
    lv_obj_add_event_cb(back_button, on_back, LV_EVENT_CLICKED, NULL);
    previous_button = create_icon_button(row, LV_SYMBOL_PREV, on_previous);
    refresh_button = create_icon_button(row, LV_SYMBOL_REFRESH, on_refresh);
    next_button = create_icon_button(row, LV_SYMBOL_NEXT, on_next);
}

lv_obj_t *ui_modules_screen_create(const modules_view_t *view, const ui_modules_handlers_t *on_events)
{
    handlers = on_events;
    screen_object = ui_screen_create();
    ui_status_bar_create(screen_object, module_title(view->kind));
    lv_obj_add_event_cb(screen_object, forget_screen, LV_EVENT_DELETE, NULL);
    content = ui_scroll_box_create(screen_object, UI_CONTENT_WIDTH_PX, CONTENT_HEIGHT_PX);
    lv_obj_set_pos(content, UI_SCREEN_MARGIN_PX, CONTENT_Y_PX);
    create_controls(screen_object);
    ui_modules_show(view);
    return screen_object;
}

/* Centered headline and message in place of the content. */
static void show_message(const char *headline, const char *message)
{
    lv_obj_t *head = create_label(content, UI_FONT_BODY, LV_TEXT_ALIGN_CENTER);
    lv_label_set_text_static(head, headline);
    if (message == NULL) return;
    lv_obj_t *text = create_label(content, UI_FONT_SECONDARY, LV_TEXT_ALIGN_CENTER);
    lv_obj_add_style(text, &ui_theme_styles()->text_secondary, LV_PART_MAIN);
    lv_label_set_text_static(text, message);
}

static void show_failure(void)
{
    char hint[96];
    const bool busy = shown.error == MODULES_ERROR_BUSY_AUDIO || shown.error == MODULES_ERROR_BUSY_BRIDGE;
    const char *headline = busy                                       ? UI_TEXT_MODULES_BUSY
                           : shown.screen == MODULES_SCREEN_DETAIL ? UI_TEXT_MODULES_FAILED_DETAIL
                                                                   : UI_TEXT_MODULES_FAILED_LIST;
    lv_obj_t *head = create_label(content, UI_FONT_BODY, LV_TEXT_ALIGN_CENTER);
    lv_label_set_text_static(head, headline);
    lv_obj_t *text = create_label(content, UI_FONT_SECONDARY, LV_TEXT_ALIGN_CENTER);
    lv_obj_add_style(text, &ui_theme_styles()->text_secondary, LV_PART_MAIN);
    snprintf(hint, sizeof(hint), "%s\n%s", error_text(shown.error), UI_TEXT_MODULES_REFRESH_HINT);
    lv_label_set_text(text, hint);
}

static void create_row(const bridge_resource_entry_t *entry, size_t index)
{
    char date[DATE_SIZE], line[DATE_SIZE + 24];
    const bool alert_time = shown.kind != BRIDGE_RESOURCE_NOTES && entry->when_at[0] != '\0';
    const char *format = shown.kind == BRIDGE_RESOURCE_ALERTS ? UI_TEXT_MODULES_ROW_SCHEDULED
                         : alert_time                          ? UI_TEXT_MODULES_ROW_ALERT
                                                               : UI_TEXT_MODULES_ROW_CREATED;
    format_date(alert_time ? entry->when_at : entry->created_at, date);
    snprintf(line, sizeof(line), format, date);
    row_ids[index] = entry->id;

    ui_styles_t *styles = ui_theme_styles();
    lv_obj_t *row = lv_button_create(content);
    lv_obj_add_style(row, &styles->button, LV_PART_MAIN);
    lv_obj_add_style(row, &styles->button_pressed, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(row, ROW_PAD_PX, LV_PART_MAIN);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(row, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_gap(row, UI_ROW_LABEL_GAP_PX, LV_PART_MAIN);
    lv_obj_add_event_cb(row, on_row, LV_EVENT_CLICKED, (void *)(uintptr_t)index);

    /* Two lines at most: the row is a summary and the detail shows the whole text. */
    lv_obj_t *text = create_label(row, UI_FONT_BODY, LV_TEXT_ALIGN_LEFT);
    lv_label_set_text(text, entry->text);
    lv_obj_update_layout(text);
    const int32_t max_height = ROW_TEXT_LINES * lv_font_get_line_height(ui_theme_font(UI_FONT_BODY));
    if (lv_obj_get_height(text) > max_height) {
        lv_label_set_long_mode(text, LV_LABEL_LONG_DOT);
        lv_obj_set_height(text, max_height);
    }
    lv_obj_t *when = create_label(row, UI_FONT_SECONDARY, LV_TEXT_ALIGN_LEFT);
    lv_obj_add_style(when, &styles->text_secondary, LV_PART_MAIN);
    lv_label_set_text(when, line);
}

static void show_rows(const bridge_resource_page_t *page, void *unused)
{
    (void)unused;
    if (page->count == 0U) {
        show_message(UI_TEXT_MODULES_EMPTY, NULL);
        return;
    }
    for (uint8_t i = 0; i < page->count; ++i) create_row(&page->entries[i], i);
}

static void show_list(void)
{
    if (shown.status == MODULES_LOADING) show_message(UI_TEXT_MODULES_LOADING, NULL);
    else if (shown.status == MODULES_FAILED) show_failure();
    else if (handlers->read_page == NULL || !handlers->read_page(shown.ticket, show_rows, NULL))
        /* The view is already stale and a newer one follows: neither empty nor failed. */
        show_message(UI_TEXT_MODULES_LOADING, NULL);
    snprintf(title_text, sizeof(title_text), UI_TEXT_MODULES_PAGE_TITLE_FORMAT, module_title(shown.kind),
             (unsigned)shown.page);
    ui_status_bar_set_title(title_text);
}

static void add_meta_line(char *meta, size_t capacity, const char *format, const char *value)
{
    const size_t used = strlen(meta);
    snprintf(meta + used, capacity - used, format, value);
}

static lv_obj_t *create_fact_label(void)
{
    lv_obj_t *facts = create_label(content, UI_FONT_SECONDARY, LV_TEXT_ALIGN_LEFT);
    lv_obj_add_style(facts, &ui_theme_styles()->text_secondary, LV_PART_MAIN);
    lv_obj_set_style_text_line_space(facts, FACT_LINE_SPACE_PX, LV_PART_MAIN);
    return facts;
}

static void show_detail_content(const bridge_resource_detail_t *detail, void *unused)
{
    (void)unused;
    /* Only bounded values go here: dates, status and the ID. Free text has a label of its own. */
    char meta[FACTS_SIZE] = "";
    char date[DATE_SIZE], id[ID_SIZE];
    snprintf(id, sizeof(id), "%llu", (unsigned long long)detail->id);

    lv_obj_t *heading = create_label(content, UI_FONT_TITLE, LV_TEXT_ALIGN_LEFT);
    lv_label_set_text(heading, detail->text);
    bool supported = ui_text_support_covers(ui_theme_font(UI_FONT_TITLE), detail->text);
    if (detail->description != NULL && detail->description[0] != '\0') {
        lv_obj_t *description = create_label(content, UI_FONT_BODY, LV_TEXT_ALIGN_LEFT);
        lv_label_set_text(description, detail->description);
        supported = supported && ui_text_support_covers(ui_theme_font(UI_FONT_BODY), detail->description);
    }
    if (detail->tag != NULL) supported = supported && ui_text_support_covers(ui_theme_font(UI_FONT_SECONDARY), detail->tag);
    if (!supported) {
        lv_obj_t *note = create_label(content, UI_FONT_SECONDARY, LV_TEXT_ALIGN_LEFT);
        lv_obj_set_style_text_color(note, lv_color_hex(UI_COLOR_WARNING_TEXT), LV_PART_MAIN);
        lv_label_set_text_static(note, UI_TEXT_MODULES_NOTE_UNSUPPORTED);
    }

    if (detail->kind == BRIDGE_RESOURCE_TASKS) {
        add_meta_line(meta, sizeof(meta), UI_TEXT_MODULES_DETAIL_STATUS "\n", task_status_text(detail->task_status));
        format_date(detail->when_at, date);
        add_meta_line(meta, sizeof(meta), UI_TEXT_MODULES_DETAIL_ALERT "\n", date);
    } else if (detail->kind == BRIDGE_RESOURCE_ALERTS) {
        format_date(detail->when_at, date);
        add_meta_line(meta, sizeof(meta), UI_TEXT_MODULES_DETAIL_SCHEDULED "\n", date);
        add_meta_line(meta, sizeof(meta), UI_TEXT_MODULES_DETAIL_SENT "\n", UI_TEXT_MODULES_SENT_NO);
    }
    format_date(detail->created_at, date);
    add_meta_line(meta, sizeof(meta), UI_TEXT_MODULES_DETAIL_CREATED "\n", date);
    if (detail->kind == BRIDGE_RESOURCE_NOTES) {
        format_date(detail->updated_at, date);
        add_meta_line(meta, sizeof(meta), UI_TEXT_MODULES_DETAIL_UPDATED "\n", date);
    }
    add_meta_line(meta, sizeof(meta), UI_TEXT_MODULES_DETAIL_ID, id);
    lv_label_set_text(create_fact_label(), meta);
    if (detail->kind == BRIDGE_RESOURCE_ALERTS) return;
    /* The tag has no length limit in the contract: it wraps in its own label and is never cut. */
    lv_obj_t *tag = create_fact_label();
    lv_label_set_text_fmt(tag, UI_TEXT_MODULES_DETAIL_TAG, detail->tag != NULL ? detail->tag : UI_TEXT_MODULES_NOT_AVAILABLE);
}

static void show_detail(void)
{
    if (shown.status == MODULES_LOADING) show_message(UI_TEXT_MODULES_LOADING, NULL);
    else if (shown.status == MODULES_FAILED) show_failure();
    else if (handlers->read_detail == NULL || !handlers->read_detail(shown.ticket, show_detail_content, NULL))
        show_message(UI_TEXT_MODULES_LOADING, NULL);
    ui_status_bar_set_title(module_title(shown.kind));
}

static void show_controls(void)
{
    const bool list = shown.screen == MODULES_SCREEN_LIST;
    const bool ready = shown.status == MODULES_READY;
    set_visible(previous_button, list);
    set_visible(next_button, list);
    set_visible(refresh_button, list || shown.status == MODULES_FAILED);
    set_enabled(previous_button, ready && shown.has_previous);
    set_enabled(next_button, ready && shown.has_next);
    set_enabled(refresh_button, shown.status != MODULES_LOADING);
}

void ui_modules_show(const modules_view_t *view)
{
    if (handlers == NULL || content == NULL) return;
    shown = *view;
    lv_obj_clean(content);
    lv_obj_scroll_to_y(content, 0, LV_ANIM_OFF);
    if (shown.screen == MODULES_SCREEN_LIST) show_list();
    else show_detail();
    show_controls();
}
