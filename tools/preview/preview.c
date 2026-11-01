/* Renders UI screens with the pinned LVGL into PNG files. Screen content here is sample data. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "board_display_geometry.h"
#include "lodepng.h"
#include "lvgl.h"
#include "ui_home.h"
#include "ui_section.h"
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
    capture(display, ui_home_screen_create(ignore_section), directory, "home");
    ui_status_bar_set_battery((ui_battery_t){.known = false});
    capture(display, ui_home_screen_create(ignore_section), directory, "home-battery-unknown");
    ui_status_bar_set_battery((ui_battery_t){.known = true, .percent = SAMPLE_BATTERY_PERCENT});
    capture(display, ui_section_screen_create(UI_SECTION_TASKS, ignore_back), directory, "section");
    capture(display, create_latin1_sample(), directory, "latin1-sample");
    return EXIT_SUCCESS;
}
