/* Renders UI screens with the pinned LVGL into PNG files. Screen content here is sample data. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "board_display_geometry.h"
#include "lodepng.h"
#include "lvgl.h"
#include "ui_home.h"
#include "ui_recording.h"
#include "ui_section.h"
#include "ui_settings.h"
#include "ui_wifi.h"
#include "ui_status_bar.h"
#include "ui_theme.h"

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

static const ui_recording_handlers_t RECORDING_HANDLERS = {
    .on_send = ignore_generation,
    .on_cancel = ignore_generation,
    .on_close = ignore_generation,
};

static lv_obj_t *create_recording(recording_phase_t phase, recording_failure_t failure, uint32_t duration_ms)
{
    const recording_view_t view = {.phase = phase, .failure = failure, .duration_ms = duration_ms, .generation = 1};
    return ui_recording_screen_create(&view, &RECORDING_HANDLERS);
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
    lv_obj_t *screen = ui_section_screen_create(UI_SECTION_NOTES, ignore_back);
    lv_obj_t *label = lv_label_create(screen);
    lv_label_set_text_static(label, LATIN1_SAMPLE_TEXT);
    lv_obj_set_width(label, UI_CONTENT_WIDTH_PX);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, UI_STATUS_BAR_HEIGHT_PX + UI_GAP_PX);
    return screen;
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
    capture(display, ui_home_screen_create(ignore_section, ignore_back), directory, "home");
    ui_status_bar_set_bridge_online(true);
    capture(display, ui_home_screen_create(ignore_section, ignore_back), directory, "home-wifi-online");
    ui_status_bar_set_bridge_online(false);
    capture(display, ui_home_screen_create(ignore_section, ignore_back), directory, "home-wifi-offline");
    ui_status_bar_set_battery((ui_battery_t){.known = false});
    capture(display, ui_home_screen_create(ignore_section, ignore_back), directory, "home-battery-unknown");
    ui_status_bar_set_battery((ui_battery_t){.known = true, .percent = SAMPLE_BATTERY_PERCENT});
    capture(display, ui_section_screen_create(UI_SECTION_TASKS, ignore_back), directory, "section");
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
    return EXIT_SUCCESS;
}
