/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 *
 * ES7210 register sequences reduced from esp_codec_dev as distributed in the Waveshare ESP32-C6-Touch-AMOLED-2.16
 * examples, through the author's previous firmware (V4); see THIRD_PARTY_NOTICES.md. I2S, PMIC and lifecycle glue
 * is local.
 */
#include "board_audio.h"

#include <stdbool.h>
#include <string.h>

#include "board_bus.h"
#include "board_i2s_pins.h"
#include "board_power.h"
#include "audio_arbiter.h"
#include "driver/i2s_std.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#define ADC_ADDRESS 0x40
#define ADC_SCL_SPEED_HZ 100000
#define AUDIO_RAIL_SETTLE_MS 10
#define AUDIO_ACQUIRE_POLL_MS 10U
#define BYTES_PER_SAMPLE sizeof(int16_t)
#define SLOTS_PER_FRAME 2U
#define BYTES_PER_FRAME (SLOTS_PER_FRAME * BYTES_PER_SAMPLE)
#define ADC_REGISTER_CLOCK_MODE 0x08
#define ADC_CLOCK_MODE_MASTER 0x01

typedef struct {
    uint8_t reg;
    uint8_t value;
} register_value_t;

/*
 * Reset, then slave mode with an external 256*fs MCLK; normal Philips I2S, 16 bits, MIC1/MIC2 enabled with 30 dB
 * input gain (0x43/0x44 = 0x1a); MIC3/MIC4 stay powered down.
 */
static const register_value_t ADC_RESET[] = {
    {0x00, 0xff}, {0x00, 0x41}, {0x01, 0x3f}, {0x09, 0x30}, {0x0a, 0x30},
    {0x23, 0x2a}, {0x22, 0x0a}, {0x20, 0x0a}, {0x21, 0x2a},
};
static const register_value_t ADC_CONFIGURE[] = {
    {0x40, 0x43}, {0x41, 0x70}, {0x42, 0x70}, {0x07, 0x20}, {0x02, 0xc1}, {0x43, 0x1a}, {0x44, 0x1a},
    {0x45, 0x00}, {0x46, 0x00}, {0x4b, 0x00}, {0x4c, 0xff}, {0x12, 0x00}, {0x11, 0x60}, {0x01, 0x34},
    {0x06, 0x00}, {0x47, 0x08}, {0x48, 0x08}, {0x49, 0xff}, {0x4a, 0xff}, {0x40, 0x43}, {0x00, 0x71},
    {0x00, 0x41},
};
static const register_value_t ADC_POWER_DOWN[] = {
    {0x47, 0xff}, {0x48, 0xff}, {0x49, 0xff}, {0x4a, 0xff}, {0x4b, 0xff},
    {0x4c, 0xff}, {0x40, 0xc0}, {0x01, 0x7f}, {0x06, 0x07},
};

static const char *TAG = "board_audio";
static i2c_master_dev_handle_t adc;
static i2s_chan_handle_t receiver;
static bool receiver_enabled;
static bool adc_configured;
static bool rail_owned;
static bool arbiter_owned;
static volatile uint32_t overruns;
/* Interleaved left/right frames of one read; cleared after each read so no audio stays behind. */
static int16_t frames[BOARD_AUDIO_BLOCK_SAMPLES * SLOTS_PER_FRAME];

static esp_err_t write_adc(uint8_t reg, uint8_t value)
{
    const uint8_t frame[] = {reg, value};
    return i2c_master_transmit(adc, frame, sizeof(frame), BOARD_I2C_TIMEOUT_MS);
}

static esp_err_t write_sequence(const register_value_t *values, size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        ESP_RETURN_ON_ERROR(write_adc(values[i].reg, values[i].value), TAG, "ES7210 0x%02x", values[i].reg);
    }
    return ESP_OK;
}

static esp_err_t configure_adc(void)
{
    adc_configured = true;
    ESP_RETURN_ON_ERROR(write_sequence(ADC_RESET, sizeof(ADC_RESET) / sizeof(ADC_RESET[0])), TAG, "reset");
    uint8_t clock_mode = 0;
    const uint8_t reg = ADC_REGISTER_CLOCK_MODE;
    ESP_RETURN_ON_ERROR(i2c_master_transmit_receive(adc, &reg, sizeof(reg), &clock_mode, sizeof(clock_mode),
                                                    BOARD_I2C_TIMEOUT_MS),
                        TAG, "clock mode");
    /* The ESP32 owns the clocks: the ADC is the slave. */
    ESP_RETURN_ON_ERROR(write_adc(ADC_REGISTER_CLOCK_MODE, (uint8_t)(clock_mode & ~ADC_CLOCK_MODE_MASTER)), TAG,
                        "clock mode");
    return write_sequence(ADC_CONFIGURE, sizeof(ADC_CONFIGURE) / sizeof(ADC_CONFIGURE[0]));
}

static bool on_receive_overflow(i2s_chan_handle_t channel, i2s_event_data_t *event, void *context)
{
    (void)channel;
    (void)event;
    (void)context;
    ++overruns;
    return false;
}

static esp_err_t start_receiver(void)
{
    i2s_chan_config_t channel = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    channel.dma_desc_num = CONFIG_BOARD_AUDIO_DMA_DESC_COUNT;
    channel.dma_frame_num = BOARD_AUDIO_BLOCK_SAMPLES;
    ESP_RETURN_ON_ERROR(i2s_new_channel(&channel, NULL, &receiver), TAG, "I2S channel");
    /* Stereo: RX mono on this chip still delivers both slots and doubles the rate. Reads keep the left slot (MIC1). */
    const i2s_std_config_t config = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(BOARD_AUDIO_SAMPLE_RATE_HZ),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg =
            {
                .mclk = BOARD_I2S_MCLK_GPIO,
                .bclk = BOARD_I2S_BCLK_GPIO,
                .ws = BOARD_I2S_WS_GPIO,
                .dout = I2S_GPIO_UNUSED,
                .din = BOARD_I2S_DIN_GPIO,
            },
    };
    ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(receiver, &config), TAG, "I2S mode");
    const i2s_event_callbacks_t callbacks = {.on_recv_q_ovf = on_receive_overflow};
    ESP_RETURN_ON_ERROR(i2s_channel_register_event_callback(receiver, &callbacks, NULL), TAG, "I2S callbacks");
    ESP_RETURN_ON_ERROR(i2s_channel_enable(receiver), TAG, "I2S enable");
    receiver_enabled = true;
    return ESP_OK;
}

static esp_err_t add_adc_device(void)
{
    if (adc != NULL) {
        return ESP_OK;
    }
    const i2c_device_config_t device = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = ADC_ADDRESS,
        .scl_speed_hz = ADC_SCL_SPEED_HZ,
    };
    return i2c_master_bus_add_device(board_bus(), &device, &adc);
}

esp_err_t board_audio_start(void)
{
    ESP_RETURN_ON_FALSE(receiver == NULL && !adc_configured && !rail_owned && !arbiter_owned,
                        ESP_ERR_INVALID_STATE, TAG,
                        "already started or not released");
    ESP_RETURN_ON_ERROR(add_adc_device(), TAG, "ES7210 device");
    for (unsigned waited = 0U; waited <= CONFIG_BOARD_AUDIO_ACQUIRE_WAIT_MS; waited += AUDIO_ACQUIRE_POLL_MS) {
        if (audio_arbiter_try_capture(board_audio_arbiter())) { arbiter_owned = true; break; }
        if (waited == CONFIG_BOARD_AUDIO_ACQUIRE_WAIT_MS) break;
        vTaskDelay(pdMS_TO_TICKS(AUDIO_ACQUIRE_POLL_MS));
    }
    ESP_RETURN_ON_FALSE(arbiter_owned, ESP_ERR_TIMEOUT, TAG, "I2S busy");
    overruns = 0;
    rail_owned = true;
    esp_err_t error = board_power_audio_rail_on();
    if (error == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(AUDIO_RAIL_SETTLE_MS));
        error = configure_adc();
    }
    if (error == ESP_OK) {
        error = start_receiver();
    }
    if (error != ESP_OK) {
        /* Keep the first failure; stop releases what it can and keeps the rest owned. */
        (void)board_audio_stop();
    }
    return error;
}

esp_err_t board_audio_read(int16_t *samples, size_t capacity, size_t *count, uint32_t timeout_ms)
{
    if (count != NULL) {
        *count = 0;
    }
    ESP_RETURN_ON_FALSE(samples != NULL && count != NULL && capacity > 0U, ESP_ERR_INVALID_ARG, TAG, "arguments");
    ESP_RETURN_ON_FALSE(receiver_enabled, ESP_ERR_INVALID_STATE, TAG, "not started");
    if (capacity > BOARD_AUDIO_BLOCK_SAMPLES) {
        capacity = BOARD_AUDIO_BLOCK_SAMPLES;
    }
    size_t bytes = 0;
    esp_err_t error = i2s_channel_read(receiver, frames, capacity * BYTES_PER_FRAME, &bytes, timeout_ms);
    if (bytes % BYTES_PER_FRAME != 0U) {
        error = ESP_ERR_INVALID_SIZE;
        bytes = 0;
    }
    *count = bytes / BYTES_PER_FRAME;
    for (size_t i = 0; i < *count; ++i) {
        samples[i] = frames[i * SLOTS_PER_FRAME];
    }
    memset(frames, 0, sizeof(frames));
    return error;
}

static void keep_first(esp_err_t *first, esp_err_t error)
{
    if (*first == ESP_OK) {
        *first = error;
    }
}

esp_err_t board_audio_stop(void)
{
    esp_err_t first = ESP_OK;
    if (receiver != NULL) {
        if (receiver_enabled) {
            const esp_err_t error = i2s_channel_disable(receiver);
            keep_first(&first, error);
            receiver_enabled = error != ESP_OK;
        }
        /* The driver refuses to delete a channel that is still running. */
        const esp_err_t error = i2s_del_channel(receiver);
        keep_first(&first, error);
        if (error == ESP_OK) {
            receiver = NULL;
            receiver_enabled = false;
        }
    }
    if (adc_configured) {
        bool powered_down = true;
        for (size_t i = 0; i < sizeof(ADC_POWER_DOWN) / sizeof(ADC_POWER_DOWN[0]); ++i) {
            const esp_err_t error = write_adc(ADC_POWER_DOWN[i].reg, ADC_POWER_DOWN[i].value);
            keep_first(&first, error);
            powered_down = powered_down && error == ESP_OK;
        }
        adc_configured = !powered_down;
    }
    if (rail_owned) {
        bool powered_off = false;
        const esp_err_t error = board_power_audio_rail_restore(&powered_off);
        keep_first(&first, error);
        rail_owned = error != ESP_OK;
        /* An unpowered ES7210 is down even if a power-down write failed. */
        if (powered_off) {
            adc_configured = false;
        }
    }
    if (arbiter_owned && receiver == NULL && !adc_configured && !rail_owned) {
        audio_arbiter_release(board_audio_arbiter());
        arbiter_owned = false;
    }
    return first;
}

uint32_t board_audio_overruns(void)
{
    return overruns;
}

void board_audio_capture_intent(bool active)
{
    audio_arbiter_capture_intent(board_audio_arbiter(), active);
}
