/* Renders UI screens with the pinned LVGL into PNG files. Screen content here is sample data. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "board_display_geometry.h"
#include "lodepng.h"
#include "lvgl.h"
#include "ui_home.h"
#include "ui_modules.h"
#include "ui_recording.h"
#include "ui_notifications.h"
#include "ui_recording_text.h"
#include "ui_section.h"
#include "ui_settings.h"
#include "ui_wifi.h"
#include "ui_status_bar.h"
#include "ui_text.h"
#include "ui_theme.h"
#include "ui_widgets.h"

#define WIDTH BOARD_DISPLAY_WIDTH_PX
#define HEIGHT BOARD_DISPLAY_HEIGHT_PX
#define DRAW_BUFFER_ROWS 48
#define RGB888_BYTES_PER_PIXEL 3
#define FRAME_TICK_MS 40
#define SAMPLE_BATTERY_PERCENT 87
#define LATIN1_SAMPLE_TEXT "Sample: Reunión mañana con Ángela.\nRevisar pingüino, café y año.\n¿Listo? ¡Sí! Ç ß Ø Æ £ °"

static uint8_t draw_buffer[WIDTH * DRAW_BUFFER_ROWS * sizeof(uint16_t)];
static uint16_t framebuffer[WIDTH * HEIGHT];
static uint8_t rgb_image[WIDTH * HEIGHT * RGB888_BYTES_PER_PIXEL];
static uint32_t now_ms;

static uint32_t tick(void)
{
    return now_ms;
}

static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixels)
{
    const int32_t width = lv_area_get_width(area);
    const uint32_t stride = lv_draw_buf_width_to_stride((uint32_t)width, LV_COLOR_FORMAT_RGB565);
    for (int32_t y = area->y1; y <= area->y2; ++y) {
        memcpy(&framebuffer[(size_t)y * WIDTH + (size_t)area->x1], pixels + (size_t)(y - area->y1) * stride,
               (size_t)width * sizeof(uint16_t));
    }
    lv_display_flush_ready(display);
}

static void round_to_panel_grid(lv_event_t *event)
{
    lv_area_t *area = lv_event_get_param(event);
    board_display_align_area(&area->x1, &area->y1, &area->x2, &area->y2);
}

static uint8_t expand_channel(uint16_t value, unsigned bits)
{
    const unsigned max = (1U << bits) - 1U;
    return (uint8_t)((value * 255U + max / 2U) / max);
}

static void write_png(const char *directory, const char *name)
{
    for (size_t i = 0; i < (size_t)WIDTH * HEIGHT; ++i) {
        const uint16_t pixel = framebuffer[i];
        uint8_t *rgb = &rgb_image[i * RGB888_BYTES_PER_PIXEL];
        rgb[0] = expand_channel(pixel >> 11, 5);
        rgb[1] = expand_channel((pixel >> 5) & 0x3f, 6);
        rgb[2] = expand_channel(pixel & 0x1f, 5);
    }

    char path[512];
    snprintf(path, sizeof(path), "%s/%s.png", directory, name);
    unsigned char *png = NULL;
    size_t png_size = 0;
    const unsigned error = lodepng_encode24(&png, &png_size, rgb_image, WIDTH, HEIGHT);
    FILE *file = error == 0 ? fopen(path, "wb") : NULL;
    const bool written = file != NULL && fwrite(png, 1, png_size, file) == png_size;
    if (file != NULL) {
        fclose(file);
    }
    lv_free(png);
    if (!written) {
        fprintf(stderr, "Could not write %s\n", path);
        exit(EXIT_FAILURE);
    }
    printf("%s\n", path);
}

static void capture(lv_display_t *display, lv_obj_t *screen, const char *directory, const char *name)
{
    lv_screen_load_anim(screen, LV_SCR_LOAD_ANIM_NONE, 0, 0, true);
    lv_obj_invalidate(screen);
    now_ms += FRAME_TICK_MS;
    lv_refr_now(display);
    write_png(directory, name);
}

static void ignore_section(ui_section_t section)
{
    (void)section;
}

static void ignore_back(lv_event_t *event)
{
    (void)event;
}

static void ignore_drag(settings_id_t id, uint8_t value)
{
    (void)id;
    (void)value;
}

static void ignore_release(settings_id_t id)
{
    (void)id;
}

static void ignore_timeout(uint8_t seconds)
{
    (void)seconds;
}

static void ignore_closed(void) {}

static const ui_settings_handlers_t SETTINGS_HANDLERS = {
    .on_drag = ignore_drag,
    .on_release = ignore_release,
    .on_select_timeout = ignore_timeout,
    .on_closed = ignore_closed,
    .on_back = ignore_back,
    .on_wifi = ignore_back,
};

static void ignore_ssid(const char *ssid)
{
    (void)ssid;
}

static void ignore_action(void) {}

static const ui_wifi_handlers_t WIFI_HANDLERS = {
    .on_forget = ignore_ssid,
    .on_add = ignore_action,
    .on_cancel_setup = ignore_action,
    .on_back = ignore_back,
};

static void ignore_generation(uint64_t generation)
{
    (void)generation;
}

static void ignore_notification_close(uint64_t generation, const char *event_id)
{
    (void)generation;
    (void)event_id;
}

static bool ignore_notification_snooze(uint64_t generation, const char *event_id, unsigned preset)
{
    (void)generation;
    (void)event_id;
    (void)preset;
    return true;
}

static const ui_notifications_handlers_t NOTIFICATION_HANDLERS = {
    .close = ignore_notification_close, .snooze = ignore_notification_snooze,
};

/* A long reply in Spanish with typographic punctuation, plus one character outside the supported set. */
static const char SAMPLE_REPLY[] =
    "Listo: agendé la reunión con Ana para el jueves a las 10:00 \u2014 te mandé la invitación.\n\n"
    "Pendientes que encontré en tu lista:\n"
    "\u2022 Revisar el presupuesto (vence mañana).\n"
    "\u2022 Llamar al proveedor por la entrega de 1.250 \u20ac.\n"
    "\u2022 Confirmar el \u201cplan B\u201d del viaje\u2026\n\n"
    "Si querés, también puedo recordarte la reunión 15 minutos antes. \u2705";
static const char *sample_reply = SAMPLE_REPLY;

static bool read_sample_reply(uint64_t generation, void (*reader)(const char *text, void *context), void *context)
{
    (void)generation;
    reader(sample_reply, context);
    return true;
}

static const ui_recording_handlers_t RECORDING_HANDLERS = {
    .on_send = ignore_generation,
    .on_cancel = ignore_generation,
    .on_close = ignore_generation,
    .on_check = ignore_generation,
    .on_discard = ignore_generation,
    .read_message = read_sample_reply,
};

static lv_obj_t *create_recording_view(const recording_view_t *view)
{
    return ui_recording_screen_create(view, &RECORDING_HANDLERS);
}

static lv_obj_t *create_recording(recording_phase_t phase, recording_failure_t failure, uint32_t duration_ms)
{
    const recording_view_t view = {.phase = phase, .failure = failure, .duration_ms = duration_ms, .generation = 1};
    return create_recording_view(&view);
}

/* Taps Discard on the open panel so the confirmation shows. */
static lv_obj_t *create_discard_confirmation(const recording_view_t *view)
{
    lv_obj_t *screen = create_recording_view(view);
    lv_obj_t *row = lv_obj_get_child(screen, -1);
    for (uint32_t i = 0; i < lv_obj_get_child_count(row); ++i) {
        lv_obj_t *button = lv_obj_get_child(row, (int32_t)i);
        if (strcmp(lv_label_get_text(lv_obj_get_child(button, -1)), "Discard") == 0 &&
            !lv_obj_has_flag(button, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_send_event(button, LV_EVENT_CLICKED, NULL);
            break;
        }
    }
    return screen;
}

static void capture_attempt_states(lv_display_t *display, const char *directory)
{
    recording_view_t ready = {.phase = RECORDING_READY_TO_SEND, .notice = RECORDING_NOTICE_NOT_CONNECTED,
                              .duration_ms = 8400, .generation = 1};
    capture(display, create_recording_view(&ready), directory, "recording-ready-not-connected");
    const recording_view_t uploading = {.phase = RECORDING_UPLOADING, .generation = 2, .close_keeps_attempt = true};
    capture(display, create_recording_view(&uploading), directory, "recording-uploading");
    const recording_view_t processing = {.phase = RECORDING_PROCESSING, .generation = 2, .close_keeps_attempt = true};
    capture(display, create_recording_view(&processing), directory, "recording-processing");
    const recording_view_t completed = {.phase = RECORDING_COMPLETED, .generation = 2, .has_message = true,
                                        .content_omitted = true};
    capture(display, create_recording_view(&completed), directory, "recording-completed");
    recording_view_t not_cleared = completed;
    not_cleared.notice = RECORDING_NOTICE_NOT_CLEARED;
    capture(display, create_recording_view(&not_cleared), directory, "recording-not-cleared");
    const recording_view_t restored = {.phase = RECORDING_COMPLETED, .generation = 2, .can_check = true};
    capture(display, create_recording_view(&restored), directory, "recording-completed-unavailable");
    sample_reply = "The assistant could not understand the audio. Try recording again in a quieter place.";
    const recording_view_t remote = {.phase = RECORDING_FAILED, .failure = RECORDING_FAILURE_REMOTE,
                                     .generation = 2, .has_message = true};
    capture(display, create_recording_view(&remote), directory, "recording-remote-failed");
    sample_reply = "The result of this request could not be confirmed by the bridge.";
    const recording_view_t unknown = {.phase = RECORDING_UNKNOWN, .generation = 2, .has_message = true};
    capture(display, create_recording_view(&unknown), directory, "recording-unknown");
    sample_reply = SAMPLE_REPLY;
    const recording_view_t unconfirmed = {.phase = RECORDING_UNCONFIRMED, .generation = 2, .can_check = true,
                                          .can_discard = true, .close_keeps_attempt = true};
    capture(display, create_recording_view(&unconfirmed), directory, "recording-unconfirmed");
    capture(display, create_discard_confirmation(&unconfirmed), directory, "recording-discard-confirm");
    const recording_view_t not_sent = {.phase = RECORDING_FAILED, .failure = RECORDING_FAILURE_NOT_SENT,
                                       .generation = 2};
    capture(display, create_recording_view(&not_sent), directory, "recording-not-sent");
    const recording_view_t record_failed = {.phase = RECORDING_UNCONFIRMED, .failure = RECORDING_FAILURE_RECORD,
                                            .generation = 2, .can_check = true, .close_keeps_attempt = true};
    capture(display, create_recording_view(&record_failed), directory, "recording-record-failed");
}

static connectivity_view_t sample_view(connectivity_state_t state, unsigned saved_count)
{
    static const char *const SSIDS[WIFI_LIST_CAPACITY] = {"Home", "Office 5G", "Casa de Ana", "Cafe Central",
                                                          "The Smiths' Home Network Upstair"};
    connectivity_view_t view = {.state = state, .saved_count = (uint8_t)saved_count};
    for (unsigned i = 0; i < saved_count; ++i) {
        snprintf(view.saved[i], sizeof(view.saved[i]), "%s", SSIDS[i]);
    }
    return view;
}

static connectivity_view_t sample_setup_view(bool password_rejected)
{
    connectivity_view_t view = sample_view(CONNECTIVITY_OFFLINE, 2);
    view.setup_active = true;
    view.setup_password_rejected = password_rejected;
    snprintf(view.setup_ssid, sizeof(view.setup_ssid), "Desk-Assistant-ED28");
    snprintf(view.setup_password, sizeof(view.setup_password), "k7m2xq9d");
    return view;
}

static lv_obj_t *create_wifi(const connectivity_view_t *view)
{
    return ui_wifi_screen_create(view, &WIFI_HANDLERS);
}

static lv_obj_t *create_wifi_with_dialog(const connectivity_view_t *view, ui_wifi_dialog_t kind, const char *ssid)
{
    lv_obj_t *screen = create_wifi(view);
    ui_wifi_open_dialog(kind, ssid);
    return screen;
}

static lv_obj_t *create_settings_with_wifi(connectivity_state_t state)
{
    settings_row_t rows[SETTINGS_COUNT];
    settings_row_init(&rows[SETTINGS_SCREEN_TIMEOUT], SETTINGS_SCREEN_TIMEOUT, SETTINGS_SCREEN_TIMEOUT_DEFAULT_S, false);
    settings_row_init(&rows[SETTINGS_VOLUME], SETTINGS_VOLUME, SETTINGS_VOLUME_DEFAULT_PERCENT, false);
    settings_row_init(&rows[SETTINGS_BRIGHTNESS], SETTINGS_BRIGHTNESS, SETTINGS_BRIGHTNESS_DEFAULT_PERCENT, false);
    lv_obj_t *screen = ui_settings_screen_create(rows, &SETTINGS_HANDLERS);
    ui_settings_show_wifi(state);
    return screen;
}

static lv_obj_t *create_settings(uint8_t timeout_s, uint8_t volume, uint8_t brightness, bool with_errors)
{
    settings_row_t rows[SETTINGS_COUNT];
    settings_row_init(&rows[SETTINGS_SCREEN_TIMEOUT], SETTINGS_SCREEN_TIMEOUT, timeout_s, with_errors);
    settings_row_init(&rows[SETTINGS_VOLUME], SETTINGS_VOLUME, volume, false);
    settings_row_init(&rows[SETTINGS_BRIGHTNESS], SETTINGS_BRIGHTNESS, brightness, false);
    rows[SETTINGS_VOLUME].save_failed = with_errors;
    rows[SETTINGS_BRIGHTNESS].apply_failed = with_errors;
    return ui_settings_screen_create(rows, &SETTINGS_HANDLERS);
}

static lv_obj_t *create_latin1_sample(void)
{
    lv_obj_t *screen = ui_screen_create();
    ui_status_bar_create(screen, UI_TEXT_NOTES);
    lv_obj_t *label = lv_label_create(screen);
    lv_label_set_text_static(label, LATIN1_SAMPLE_TEXT);
    lv_obj_set_width(label, UI_CONTENT_WIDTH_PX);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, UI_STATUS_BAR_HEIGHT_PX + UI_GAP_PX);
    return screen;
}


/* Sample module data: the readers hand the screen whatever the current sample holds. */
static bridge_resource_page_t sample_page;
static bridge_resource_detail_t sample_detail;

static bool read_sample_page(uint64_t ticket, void (*reader)(const bridge_resource_page_t *page, void *context),
                             void *context)
{
    (void)ticket;
    reader(&sample_page, context);
    return true;
}

static bool read_sample_detail(uint64_t ticket, void (*reader)(const bridge_resource_detail_t *detail, void *context),
                               void *context)
{
    (void)ticket;
    reader(&sample_detail, context);
    return true;
}

static void ignore_ticket(uint64_t ticket)
{
    (void)ticket;
}

static void ignore_open(uint64_t ticket, uint64_t id)
{
    (void)ticket;
    (void)id;
}

static const ui_modules_handlers_t MODULES_HANDLERS = {
    .on_leave = ignore_closed,
    .on_previous = ignore_ticket,
    .on_next = ignore_ticket,
    .on_refresh = ignore_ticket,
    .on_open = ignore_open,
    .on_back = ignore_ticket,
    .read_page = read_sample_page,
    .read_detail = read_sample_detail,
};

static void sample_entry(bridge_resource_entry_t *entry, uint64_t id, const char *text, const char *created,
                         const char *when)
{
    memset(entry, 0, sizeof(*entry));
    entry->id = id;
    entry->text = text;
    snprintf(entry->created_at, sizeof(entry->created_at), "%s", created);
    snprintf(entry->when_at, sizeof(entry->when_at), "%s", when);
}

static modules_view_t modules_view(bridge_resource_kind_t kind, modules_screen_t screen, modules_status_t status,
                                   modules_error_t error)
{
    return (modules_view_t){.open = true, .kind = kind, .screen = screen, .status = status, .error = error,
                            .ticket = 1, .page = 2, .has_previous = true, .has_next = true, .count = 5};
}

static lv_obj_t *create_modules(const modules_view_t *view)
{
    return ui_modules_screen_create(view, &MODULES_HANDLERS);
}

static void capture_module_lists(lv_display_t *display, const char *directory)
{
    memset(&sample_page, 0, sizeof(sample_page));
    sample_page.count = 5;
    sample_entry(&sample_page.entries[0], 41, "Ideas para el viaje a Córdoba y lista de cosas para llevar en la mochila — incluir cargadores",
                 "2026-11-02T17:05:00Z", "");
    sample_entry(&sample_page.entries[1], 40, "Receta de la abuela", "2026-11-01T01:30:00Z", "");
    sample_entry(&sample_page.entries[2], 39, "Presupuesto", "2026-10-31T12:00:00Z", "");
    sample_entry(&sample_page.entries[3], 38, "Reunión del jueves", "2026-10-30T12:00:00Z", "");
    sample_entry(&sample_page.entries[4], 37, "Libros pendientes", "2026-10-29T12:00:00Z", "");
    capture(display, create_modules(&(modules_view_t){.open = true, .kind = BRIDGE_RESOURCE_NOTES, .ticket = 1,
                                                       .status = MODULES_READY, .page = 1, .has_next = true, .count = 5}),
            directory, "modules-notes-first");
    const modules_view_t notes = modules_view(BRIDGE_RESOURCE_NOTES, MODULES_SCREEN_LIST, MODULES_READY, MODULES_ERROR_NONE);
    capture(display, create_modules(&notes), directory, "modules-notes-middle");

    sample_entry(&sample_page.entries[0], 52, "Llamar al proveedor por la entrega", "2026-11-02T17:05:00Z",
                 "2026-11-03T12:00:00Z");
    sample_entry(&sample_page.entries[1], 51, "Pagar la factura de luz", "2026-11-01T17:05:00Z", "");
    const modules_view_t tasks = modules_view(BRIDGE_RESOURCE_TASKS, MODULES_SCREEN_LIST, MODULES_READY, MODULES_ERROR_NONE);
    capture(display, create_modules(&tasks), directory, "modules-tasks");

    sample_entry(&sample_page.entries[0], 62, "Reunión con el equipo de diseño", "2026-11-02T17:05:00Z",
                 "2026-11-03T13:00:00.250Z");
    const modules_view_t alerts = modules_view(BRIDGE_RESOURCE_ALERTS, MODULES_SCREEN_LIST, MODULES_READY, MODULES_ERROR_NONE);
    capture(display, create_modules(&alerts), directory, "modules-alerts");

    sample_page.count = 0;
    modules_view_t empty = modules_view(BRIDGE_RESOURCE_ALERTS, MODULES_SCREEN_LIST, MODULES_READY, MODULES_ERROR_NONE);
    empty.page = 1;
    empty.has_previous = empty.has_next = false;
    empty.count = 0;
    capture(display, create_modules(&empty), directory, "modules-empty");

    modules_view_t loading = modules_view(BRIDGE_RESOURCE_NOTES, MODULES_SCREEN_LIST, MODULES_LOADING, MODULES_ERROR_NONE);
    loading.has_previous = loading.has_next = false;
    capture(display, create_modules(&loading), directory, "modules-loading");
    modules_view_t failed = modules_view(BRIDGE_RESOURCE_NOTES, MODULES_SCREEN_LIST, MODULES_FAILED, MODULES_ERROR_NETWORK);
    failed.has_previous = failed.has_next = false;
    capture(display, create_modules(&failed), directory, "modules-error-network");
    failed.error = MODULES_ERROR_BUSY_AUDIO;
    capture(display, create_modules(&failed), directory, "modules-busy-recording");
    failed.error = MODULES_ERROR_TOO_LARGE;
    failed.screen = MODULES_SCREEN_DETAIL;
    capture(display, create_modules(&failed), directory, "modules-detail-too-large");
    failed.error = MODULES_ERROR_NOT_FOUND;
    capture(display, create_modules(&failed), directory, "modules-detail-not-found");
}

static void sample_detail_base(bridge_resource_kind_t kind, uint64_t id, const char *text)
{
    memset(&sample_detail, 0, sizeof(sample_detail));
    sample_detail.kind = kind;
    sample_detail.id = id;
    sample_detail.text = text;
    snprintf(sample_detail.created_at, sizeof(sample_detail.created_at), "2026-11-02T17:05:00Z");
}

static void capture_module_details(lv_display_t *display, const char *directory)
{
    static const char LONG_DESCRIPTION[] =
        "Confirmar el envío de los cables USB-C y pedir factura A.\n\nPuntos a tratar:\n"
        "• Precio por cantidad (mínimo 50 unidades).\n• Plazo de entrega: 10 días hábiles.\n"
        "• Garantía de 12 meses y cambio por defectos.\n• Forma de pago: transferencia a 30 días.\n"
        "• Dirección de entrega: depósito central, puerta 4.\n\nSi no responde antes del viernes, "
        "llamar de nuevo y consultar por la alternativa del otro proveedor.";
    const modules_view_t detail = modules_view(BRIDGE_RESOURCE_TASKS, MODULES_SCREEN_DETAIL, MODULES_READY, MODULES_ERROR_NONE);

    sample_detail_base(BRIDGE_RESOURCE_TASKS, 52, "Llamar al proveedor");
    sample_detail.description = LONG_DESCRIPTION;
    sample_detail.tag = "work-with-a-very-long-tag-name-that-wraps-over-several-lines-and-must-never-be-cut-short-by-the-screen";
    sample_detail.task_status = BRIDGE_TASK_IN_PROGRESS;
    snprintf(sample_detail.when_at, sizeof(sample_detail.when_at), "2026-11-03T12:00:00Z");
    capture(display, create_modules(&detail), directory, "modules-detail-task");

    sample_detail_base(BRIDGE_RESOURCE_TASKS, 53, "Comprar pan ✅");
    sample_detail.description = "Sin alerta ni tag. Emoji ✅ sin glifo.";
    modules_view_t task_view = detail;
    capture(display, create_modules(&task_view), directory, "modules-detail-unsupported");

    sample_detail_base(BRIDGE_RESOURCE_NOTES, 41, "Ideas para el viaje");
    sample_detail.description = "";
    snprintf(sample_detail.updated_at, sizeof(sample_detail.updated_at), "2026-11-02T18:30:00Z");
    modules_view_t note_view = modules_view(BRIDGE_RESOURCE_NOTES, MODULES_SCREEN_DETAIL, MODULES_READY, MODULES_ERROR_NONE);
    capture(display, create_modules(&note_view), directory, "modules-detail-note");

    sample_detail_base(BRIDGE_RESOURCE_ALERTS, 62, "Reunión con el equipo de diseño en la sala 3");
    snprintf(sample_detail.when_at, sizeof(sample_detail.when_at), "2026-11-03T13:00:00.250Z");
    modules_view_t alert_view = modules_view(BRIDGE_RESOURCE_ALERTS, MODULES_SCREEN_DETAIL, MODULES_READY, MODULES_ERROR_NONE);
    capture(display, create_modules(&alert_view), directory, "modules-detail-alert");
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s OUTPUT_DIRECTORY\n", argv[0]);
        return EXIT_FAILURE;
    }
    const char *directory = argv[1];

    lv_init();
    lv_tick_set_cb(tick);
    lv_display_t *display = lv_display_create(WIDTH, HEIGHT);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display, draw_buffer, NULL, sizeof(draw_buffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush);
    lv_display_add_event_cb(display, round_to_panel_grid, LV_EVENT_INVALIDATE_AREA, NULL);
    ui_theme_init();

    ui_status_bar_set_battery((ui_battery_t){.known = true, .percent = SAMPLE_BATTERY_PERCENT});
    capture(display, ui_home_screen_create(ignore_section, ignore_back, UI_TEXT_RECORD), directory, "home");
    ui_status_bar_set_bridge_online(true);
    capture(display, ui_home_screen_create(ignore_section, ignore_back, UI_TEXT_RECORD), directory, "home-wifi-online");
    ui_status_bar_set_bridge_online(false);
    capture(display, ui_home_screen_create(ignore_section, ignore_back, UI_TEXT_RECORD), directory, "home-wifi-offline");
    ui_status_bar_set_battery((ui_battery_t){.known = false});
    capture(display, ui_home_screen_create(ignore_section, ignore_back, UI_TEXT_RECORD), directory, "home-battery-unknown");
    ui_status_bar_set_battery((ui_battery_t){.known = true, .percent = SAMPLE_BATTERY_PERCENT});
    capture(display, create_latin1_sample(), directory, "latin1-sample");
    capture(display,
            create_settings(SETTINGS_SCREEN_TIMEOUT_DEFAULT_S, SETTINGS_VOLUME_DEFAULT_PERCENT,
                            SETTINGS_BRIGHTNESS_DEFAULT_PERCENT, false),
            directory, "settings");
    capture(display,
            create_settings(SETTINGS_SCREEN_TIMEOUT_ALWAYS_ON_S, SETTINGS_VOLUME_MIN_PERCENT,
                            SETTINGS_BRIGHTNESS_MIN_PERCENT, false),
            directory, "settings-minimum");
    capture(display,
            create_settings(15, SETTINGS_VOLUME_MAX_PERCENT, SETTINGS_BRIGHTNESS_MAX_PERCENT, false),
            directory, "settings-maximum");
    capture(display,
            create_settings(SETTINGS_SCREEN_TIMEOUT_DEFAULT_S, SETTINGS_VOLUME_DEFAULT_PERCENT,
                            SETTINGS_BRIGHTNESS_DEFAULT_PERCENT, true),
            directory, "settings-errors");

    const connectivity_view_t no_networks = sample_view(CONNECTIVITY_NO_NETWORKS, 0);
    capture(display, create_wifi(&no_networks), directory, "wifi-no-networks");
    const connectivity_view_t searching = sample_view(CONNECTIVITY_SEARCHING, 3);
    capture(display, create_wifi(&searching), directory, "wifi-searching");
    connectivity_view_t connected = sample_view(CONNECTIVITY_CONNECTED, WIFI_LIST_CAPACITY);
    connected.connected_index = 1;
    capture(display, create_wifi(&connected), directory, "wifi-connected");
    const connectivity_view_t offline = sample_view(CONNECTIVITY_OFFLINE, 2);
    capture(display, create_wifi(&offline), directory, "wifi-offline");
    connectivity_view_t list_failed = sample_view(CONNECTIVITY_NO_NETWORKS, 0);
    list_failed.list_failed = true;
    capture(display, create_wifi(&list_failed), directory, "wifi-list-failed");
    const connectivity_view_t setup = sample_setup_view(false);
    capture(display, create_wifi(&setup), directory, "wifi-setup");
    const connectivity_view_t rejected = sample_setup_view(true);
    capture(display, create_wifi(&rejected), directory, "wifi-setup-password-rejected");
    capture(display, create_wifi_with_dialog(&connected, UI_WIFI_DIALOG_FORGET, connected.saved[4]), directory,
            "wifi-dialog-forget");
    capture(display, create_wifi_with_dialog(&connected, UI_WIFI_DIALOG_ADD, NULL), directory, "wifi-dialog-add");
    capture(display, create_settings_with_wifi(CONNECTIVITY_CONNECTED), directory, "settings-wifi-connected");
    capture(display, create_settings_with_wifi(CONNECTIVITY_NO_NETWORKS), directory, "settings-wifi-no-networks");

    capture(display, create_recording(RECORDING_RECORDING, RECORDING_FAILURE_NONE, 12400), directory, "recording");
    capture(display, create_recording(RECORDING_FINALIZING, RECORDING_FAILURE_NONE, 30000), directory,
            "recording-finalizing");
    capture(display, create_recording(RECORDING_READY_TO_SEND, RECORDING_FAILURE_NONE, 30000), directory,
            "recording-ready");
    capture(display, create_recording(RECORDING_CANCELLING, RECORDING_FAILURE_NONE, 0), directory,
            "recording-cancelling");
    capture(display, create_recording(RECORDING_FAILED, RECORDING_FAILURE_TOO_SHORT, 300), directory,
            "recording-too-short");
    capture(display, create_recording(RECORDING_FAILED, RECORDING_FAILURE_CLEANUP, 0), directory,
            "recording-cleanup-failed");
    capture_attempt_states(display, directory);
    capture_module_lists(display, directory);
    capture_module_details(display, directory);
    capture(display, ui_home_screen_create(ignore_section, ignore_back, UI_TEXT_RECORDING_ENTRY_PENDING), directory,
            "home-entry-pending");

    capture(display, ui_home_screen_create(ignore_section, ignore_back, UI_TEXT_RECORD), directory,
            "home-beneath-notification");
    notification_view_t notification = {.count = 1, .generation = 1};
    strcpy(notification.card.event_id, "01234567-89ab-cdef-0123-456789abcdef");
    strcpy(notification.card.message,
           "Meeting reminder: review the proposal with Ana at 15:00. Bring the revised budget and notes.");
    ui_notifications_show(&notification, &NOTIFICATION_HANDLERS);
    lv_refr_now(display);
    write_png(directory, "notification-single");
    notification.count = 2;
    ui_notifications_show(&notification, &NOTIFICATION_HANDLERS);
    lv_refr_now(display);
    write_png(directory, "notification-next");
    lv_obj_t *card = lv_obj_get_child(lv_obj_get_child(lv_layer_top(), -1), 0);
    lv_obj_t *actions = lv_obj_get_child(card, 3);
    lv_obj_send_event(lv_obj_get_child(actions, 1), LV_EVENT_CLICKED, NULL);
    lv_refr_now(display);
    write_png(directory, "notification-snooze-presets");
    notification.action_busy = true;
    ui_notifications_show(&notification, &NOTIFICATION_HANDLERS);
    lv_refr_now(display);
    write_png(directory, "notification-snooze-busy");
    notification.action_busy = false;
    notification.notice = NOTIFICATION_VIEW_STORAGE_ERROR;
    ui_notifications_show(&notification, &NOTIFICATION_HANDLERS);
    lv_refr_now(display);
    write_png(directory, "notification-storage-error");
    notification.notice = NOTIFICATION_VIEW_SNOOZE_UNCONFIRMED;
    ui_notifications_show(&notification, &NOTIFICATION_HANDLERS);
    lv_refr_now(display);
    write_png(directory, "notification-snooze-unconfirmed");
    notification.notice = NOTIFICATION_VIEW_SNOOZE_CONFIRMED;
    notification.snooze_close_pending = true;
    strcpy(notification.snoozed_at_text, "2026-11-02 10:00 UTC-3");
    ui_notifications_show(&notification, &NOTIFICATION_HANDLERS);
    lv_refr_now(display);
    write_png(directory, "notification-snooze-close-pending");
    notification.snooze_close_pending = false;
    notification.count = 0;
    memset(&notification.card, 0, sizeof(notification.card));
    ui_notifications_show(&notification, &NOTIFICATION_HANDLERS);
    lv_refr_now(display);
    write_png(directory, "notification-snooze-confirmed");
    ui_notifications_hide();
    return EXIT_SUCCESS;
}
