/*
 * QSPI command framing derived from Espressif's esp_lcd_sh8601 driver.
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 * Modified for this project; panel init values from the board manufacturer. See THIRD_PARTY_NOTICES.md.
 */
#include "board_display.h"

#include "display_brightness.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_lcd_io_spi.h"
#include "esp_lcd_panel_io.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#define PANEL_SPI_HOST SPI2_HOST
#define PANEL_CLK_GPIO GPIO_NUM_0
#define PANEL_DATA0_GPIO GPIO_NUM_1
#define PANEL_DATA1_GPIO GPIO_NUM_2
#define PANEL_DATA2_GPIO GPIO_NUM_3
#define PANEL_DATA3_GPIO GPIO_NUM_4
#define PANEL_CS_GPIO GPIO_NUM_15
#define PANEL_PIXEL_CLOCK_HZ 40000000
#define PANEL_COMMAND_BITS 32
#define PANEL_PARAMETER_BITS 8

#define QSPI_OPCODE_WRITE_COMMAND 0x02U
#define QSPI_OPCODE_WRITE_COLOR 0x32U

enum {
    COMMAND_SLEEP_OUT = 0x11,
    COMMAND_DISPLAY_OFF = 0x28,
    COMMAND_DISPLAY_ON = 0x29,
    COMMAND_COLUMN_ADDRESS = 0x2a,
    COMMAND_ROW_ADDRESS = 0x2b,
    COMMAND_MEMORY_WRITE = 0x2c,
    COMMAND_BRIGHTNESS = 0x51,
};

#define BUFFER_BYTES (BOARD_DISPLAY_WIDTH_PX * CONFIG_BOARD_DISPLAY_BUFFER_ROWS * sizeof(uint16_t))
_Static_assert(CONFIG_BOARD_DISPLAY_BUFFER_ROWS % 2 == 0, "draw buffer rows must keep the 2x2 grid");

typedef struct {
    uint8_t command;
    uint8_t length;
    uint8_t data[4];
    uint16_t delay_ms;
} panel_init_step_t;

/* Manufacturer sequence for this panel: RGB565, full 480x480 window, maximum brightness until configured. */
static const panel_init_step_t PANEL_INIT_SEQUENCE[] = {
    {0x36, 1, {0x00}, 0},
    {0x3a, 1, {0x55}, 0},
    {COMMAND_SLEEP_OUT, 0, {0}, 600},
    {0xfe, 1, {0x20}, 0},
    {0x19, 1, {0x10}, 0},
    {0x1c, 1, {0xa0}, 0},
    {0xfe, 1, {0x00}, 0},
    {0xc4, 1, {0x80}, 0},
    {0x3a, 1, {0x55}, 0},
    {0x35, 1, {0x00}, 0},
    {0x36, 1, {0x30}, 0},
    {0x53, 1, {0x20}, 0},
    {COMMAND_BRIGHTNESS, 1, {0xff}, 0},
    {0x63, 1, {0xff}, 0},
    {COMMAND_COLUMN_ADDRESS, 4, {0x00, 0x00, 0x01, 0xdf}, 0},
    {COMMAND_ROW_ADDRESS, 4, {0x00, 0x00, 0x01, 0xdf}, 0},
    {COMMAND_DISPLAY_ON, 0, {0}, 100},
};

static const char *TAG = "board_display";

typedef struct {
    board_isr_cb_t on_transfer_done;
    void *context;
} transfer_listener_t;

static esp_lcd_panel_io_handle_t panel_io;
static transfer_listener_t transfer_listener;

static int qspi_command(uint8_t opcode, uint8_t command)
{
    return (int)(((uint32_t)opcode << 24) | ((uint32_t)command << 8));
}

static esp_err_t send_command(uint8_t command, const void *data, size_t length)
{
    return esp_lcd_panel_io_tx_param(panel_io, qspi_command(QSPI_OPCODE_WRITE_COMMAND, command), data, length);
}

static esp_err_t run_init_sequence(void)
{
    for (size_t i = 0; i < sizeof(PANEL_INIT_SEQUENCE) / sizeof(PANEL_INIT_SEQUENCE[0]); ++i) {
        const panel_init_step_t *step = &PANEL_INIT_SEQUENCE[i];
        ESP_RETURN_ON_ERROR(send_command(step->command, step->data, step->length), TAG, "init 0x%02x", step->command);
        if (step->delay_ms > 0) {
            vTaskDelay(pdMS_TO_TICKS(step->delay_ms));
        }
    }
    return ESP_OK;
}

static bool notify_transfer_done(esp_lcd_panel_io_handle_t io, esp_lcd_panel_io_event_data_t *event, void *unused)
{
    (void)io;
    (void)event;
    (void)unused;
    transfer_listener.on_transfer_done(transfer_listener.context);
    return false;
}

static esp_err_t allocate_buffers(board_display_buffers_t *buffers)
{
    *buffers = (board_display_buffers_t){.size_bytes = BUFFER_BYTES};
    buffers->first = heap_caps_malloc(BUFFER_BYTES, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (CONFIG_BOARD_DISPLAY_BUFFER_COUNT > 1) {
        buffers->second = heap_caps_malloc(BUFFER_BYTES, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    }
    const bool allocated = buffers->first != NULL && (CONFIG_BOARD_DISPLAY_BUFFER_COUNT == 1 || buffers->second != NULL);
    if (!allocated) {
        heap_caps_free(buffers->first);
        heap_caps_free(buffers->second);
        *buffers = (board_display_buffers_t){0};
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t board_display_init(board_isr_cb_t on_transfer_done, void *context, board_display_buffers_t *buffers)
{
    ESP_RETURN_ON_FALSE(on_transfer_done != NULL && buffers != NULL, ESP_ERR_INVALID_ARG, TAG, "arguments");
    ESP_RETURN_ON_FALSE(panel_io == NULL, ESP_ERR_INVALID_STATE, TAG, "already initialized");
    transfer_listener = (transfer_listener_t){.on_transfer_done = on_transfer_done, .context = context};

    const spi_bus_config_t bus = {
        .sclk_io_num = PANEL_CLK_GPIO,
        .data0_io_num = PANEL_DATA0_GPIO,
        .data1_io_num = PANEL_DATA1_GPIO,
        .data2_io_num = PANEL_DATA2_GPIO,
        .data3_io_num = PANEL_DATA3_GPIO,
        .max_transfer_sz = BUFFER_BYTES,
    };
    const esp_lcd_panel_io_spi_config_t io = {
        .cs_gpio_num = PANEL_CS_GPIO,
        .dc_gpio_num = -1,
        .spi_mode = 0,
        .pclk_hz = PANEL_PIXEL_CLOCK_HZ,
        .trans_queue_depth = CONFIG_BOARD_DISPLAY_BUFFER_COUNT,
        .on_color_trans_done = notify_transfer_done,
        .lcd_cmd_bits = PANEL_COMMAND_BITS,
        .lcd_param_bits = PANEL_PARAMETER_BITS,
        .flags.quad_mode = true,
    };

    ESP_RETURN_ON_ERROR(allocate_buffers(buffers), TAG, "draw buffers");
    esp_err_t error = spi_bus_initialize(PANEL_SPI_HOST, &bus, SPI_DMA_CH_AUTO);
    const bool bus_ready = error == ESP_OK;
    if (error == ESP_OK) {
        error = esp_lcd_new_panel_io_spi(PANEL_SPI_HOST, &io, &panel_io);
    }
    if (error == ESP_OK) {
        error = run_init_sequence();
    }
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "init failed: %s", esp_err_to_name(error));
        if (panel_io != NULL) {
            esp_lcd_panel_io_del(panel_io);
            panel_io = NULL;
        }
        if (bus_ready) {
            spi_bus_free(PANEL_SPI_HOST);
        }
        heap_caps_free(buffers->first);
        heap_caps_free(buffers->second);
        *buffers = (board_display_buffers_t){0};
    }
    return error;
}

static void encode_range(uint8_t range[4], int32_t start, int32_t end)
{
    range[0] = (uint8_t)(start >> 8);
    range[1] = (uint8_t)start;
    range[2] = (uint8_t)(end >> 8);
    range[3] = (uint8_t)end;
}

static void swap_bytes_for_panel(uint8_t *pixels, size_t pixel_count)
{
    for (size_t i = 0; i < pixel_count; ++i) {
        const uint8_t high = pixels[2 * i];
        pixels[2 * i] = pixels[2 * i + 1];
        pixels[2 * i + 1] = high;
    }
}

esp_err_t board_display_draw(int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint8_t *pixels)
{
    ESP_RETURN_ON_FALSE(panel_io != NULL, ESP_ERR_INVALID_STATE, TAG, "not initialized");
    uint8_t columns[4];
    uint8_t rows[4];
    encode_range(columns, x1, x2);
    encode_range(rows, y1, y2);
    ESP_RETURN_ON_ERROR(send_command(COMMAND_COLUMN_ADDRESS, columns, sizeof(columns)), TAG, "columns");
    ESP_RETURN_ON_ERROR(send_command(COMMAND_ROW_ADDRESS, rows, sizeof(rows)), TAG, "rows");

    const size_t pixel_count = (size_t)(x2 - x1 + 1) * (size_t)(y2 - y1 + 1);
    swap_bytes_for_panel(pixels, pixel_count);
    return esp_lcd_panel_io_tx_color(panel_io, qspi_command(QSPI_OPCODE_WRITE_COLOR, COMMAND_MEMORY_WRITE), pixels,
                                     pixel_count * sizeof(uint16_t));
}

esp_err_t board_display_set_on(bool on)
{
    ESP_RETURN_ON_FALSE(panel_io != NULL, ESP_ERR_INVALID_STATE, TAG, "not initialized");
    return send_command(on ? COMMAND_DISPLAY_ON : COMMAND_DISPLAY_OFF, NULL, 0);
}

esp_err_t board_display_set_brightness(unsigned percent)
{
    ESP_RETURN_ON_FALSE(panel_io != NULL, ESP_ERR_INVALID_STATE, TAG, "not initialized");
    ESP_RETURN_ON_FALSE(percent <= DISPLAY_BRIGHTNESS_MAX_PERCENT, ESP_ERR_INVALID_ARG, TAG, "percent");
    const uint8_t value = display_brightness_panel_value(percent);
    return send_command(COMMAND_BRIGHTNESS, &value, sizeof(value));
}
