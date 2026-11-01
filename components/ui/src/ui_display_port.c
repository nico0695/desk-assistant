#include "ui_display_port.h"

#include "board_display.h"
#include "board_touch.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "lvgl.h"

#define FLUSH_TIMEOUT_MS 100
#define TOUCH_WARNING_INTERVAL_MS 1000

static const char *TAG = "ui_display";

static lv_display_t *display;
static lv_indev_t *touch_input;
static SemaphoreHandle_t transfer_done;
static uint32_t last_touch_warning_ms;
static bool touch_warning_logged;

static void signal_transfer_done(void *unused)
{
    (void)unused;
    BaseType_t task_woken = pdFALSE;
    xSemaphoreGiveFromISR(transfer_done, &task_woken);
    portYIELD_FROM_ISR(task_woken);
}

/* Each started transfer gives the semaphore once and LVGL waits once before reusing a buffer. */
static void wait_for_transfer(lv_display_t *unused)
{
    (void)unused;
    if (xSemaphoreTake(transfer_done, pdMS_TO_TICKS(FLUSH_TIMEOUT_MS)) != pdTRUE) {
        ESP_LOGW(TAG, "panel transfer timed out");
    }
}

static void flush(lv_display_t *flushed_display, const lv_area_t *area, uint8_t *pixels)
{
    const esp_err_t error = board_display_draw(area->x1, area->y1, area->x2, area->y2, pixels);
    if (error != ESP_OK) {
        ESP_LOGW(TAG, "panel transfer failed: %s", esp_err_to_name(error));
        /* No transfer will signal completion, so release the buffer now. */
        lv_display_flush_ready(flushed_display);
    }
}

static void round_to_panel_grid(lv_event_t *event)
{
    lv_area_t *area = lv_event_get_param(event);
    board_display_align_area(&area->x1, &area->y1, &area->x2, &area->y2);
}

static void warn_touch_error(esp_err_t error)
{
    if (touch_warning_logged && lv_tick_elaps(last_touch_warning_ms) < TOUCH_WARNING_INTERVAL_MS) {
        return;
    }
    ESP_LOGW(TAG, "touch sample unavailable: %s", esp_err_to_name(error));
    touch_warning_logged = true;
    last_touch_warning_ms = lv_tick_get();
}

static void read_touch(lv_indev_t *unused, lv_indev_data_t *data)
{
    (void)unused;
    board_touch_sample_t sample;
    const esp_err_t error = board_touch_read(&sample);
    if (error != ESP_OK) {
        warn_touch_error(error);
    }
    data->state = sample.pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    data->point.x = sample.x;
    data->point.y = sample.y;
}

static esp_err_t register_display(void)
{
    board_display_buffers_t buffers;
    ESP_RETURN_ON_ERROR(board_display_init(signal_transfer_done, NULL, &buffers), TAG, "panel");
    display = lv_display_create(BOARD_DISPLAY_WIDTH_PX, BOARD_DISPLAY_HEIGHT_PX);
    ESP_RETURN_ON_FALSE(display != NULL, ESP_ERR_NO_MEM, TAG, "LVGL display");
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display, buffers.first, buffers.second, buffers.size_bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush);
    lv_display_set_flush_wait_cb(display, wait_for_transfer);
    lv_display_add_event_cb(display, round_to_panel_grid, LV_EVENT_INVALIDATE_AREA, NULL);
    return ESP_OK;
}

static esp_err_t register_touch(board_isr_cb_t on_touch_interrupt, void *context)
{
    touch_input = lv_indev_create();
    ESP_RETURN_ON_FALSE(touch_input != NULL, ESP_ERR_NO_MEM, TAG, "LVGL input");
    lv_indev_set_type(touch_input, LV_INDEV_TYPE_POINTER);
    lv_indev_set_display(touch_input, display);
    lv_indev_set_read_cb(touch_input, read_touch);
    lv_indev_set_mode(touch_input, LV_INDEV_MODE_EVENT);
    return board_touch_init(on_touch_interrupt, context);
}

esp_err_t ui_display_port_init(board_isr_cb_t on_touch_interrupt, void *context)
{
    ESP_RETURN_ON_FALSE(display == NULL, ESP_ERR_INVALID_STATE, TAG, "already initialized");
    transfer_done = xSemaphoreCreateBinary();
    ESP_RETURN_ON_FALSE(transfer_done != NULL, ESP_ERR_NO_MEM, TAG, "transfer semaphore");
    ESP_RETURN_ON_ERROR(register_display(), TAG, "display");
    return register_touch(on_touch_interrupt, context);
}

void ui_display_port_read_touch(void)
{
    lv_indev_read(touch_input);
}
