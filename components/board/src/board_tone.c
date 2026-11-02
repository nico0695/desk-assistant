/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 * ES8311 sequence adapted from the board example through the author's V4 firmware; see THIRD_PARTY_NOTICES.md.
 */
#include "board_tone.h"

#include <stdint.h>

#include "audio_arbiter.h"
#include "board_audio.h"
#include "board_bus.h"
#include "board_power.h"
#include "driver/i2s_std.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define CODEC_ADDRESS 0x18
#define CODEC_SCL_SPEED_HZ 100000
#define TONE_RAIL_SETTLE_MS 10U
#define TONE_DRAIN_MS 100U
#define TONE_WRITE_TIMEOUT_MS 100U
#define TONE_FRAMES_PER_BLOCK 160U
#define TONE_TOTAL_FRAMES 3200U
#define TONE_NOTE_FRAMES 1440U
#define TONE_GAP_FRAMES 320U
#define TONE_FADE_FRAMES 160U
#define TONE_FIRST_FREQUENCY_HZ 800U
#define TONE_SECOND_FREQUENCY_HZ 1200U
#define TONE_WAVE_SIZE 16U
#define TONE_PCM_GAIN 4U
#define TONE_MAX_VOLUME_PERCENT 100U

typedef struct { uint8_t reg, value; } register_value_t;

static const register_value_t CODEC_START[] = {
    {0x44,0x08},{0x44,0x08},{0x01,0x30},{0x02,0x00},{0x03,0x10},{0x16,0x24},
    {0x04,0x20},{0x05,0x00},{0x0b,0x00},{0x0c,0x00},{0x10,0x1f},{0x11,0x7f},
    {0x00,0x80},{0x01,0x3f},{0x08,0xff},{0x13,0x10},{0x1b,0x0a},{0x1c,0x6a},
    {0x44,0x08},{0x17,0xbf},{0x0e,0x02},{0x12,0x00},{0x14,0x1a},{0x0d,0x01},
    {0x15,0x40},{0x37,0x08},{0x45,0x00},{0x32,0xc0},
};
static const register_value_t CODEC_STOP[] = {
    {0x32,0x00},{0x0e,0xff},{0x12,0x02},{0x14,0x00},{0x0d,0xfa},{0x15,0x00},
    {0x02,0x10},{0x00,0x00},{0x00,0x1f},{0x01,0x30},{0x01,0x00},{0x45,0x00},
    {0x0d,0xfc},{0x02,0x00},
};
static const int16_t WAVE[TONE_WAVE_SIZE] = {
    0,392,724,946,1024,946,724,392,0,-392,-724,-946,-1024,-946,-724,-392,
};

static i2c_master_dev_handle_t codec;
static i2s_chan_handle_t transmitter;
static bool claimed, rails_owned, codec_touched, tx_enabled;
static bool codec_probed;
static const char *TAG = "board_tone";

static esp_err_t write_codec(uint8_t reg, uint8_t value)
{
    const uint8_t bytes[] = {reg, value};
    return i2c_master_transmit(codec, bytes, sizeof(bytes), BOARD_I2C_TIMEOUT_MS);
}

static esp_err_t update_codec(uint8_t reg, uint8_t mask, uint8_t value)
{
    uint8_t previous;
    const esp_err_t read = i2c_master_transmit_receive(codec, &reg, 1U, &previous, 1U, BOARD_I2C_TIMEOUT_MS);
    if (read != ESP_OK) return read;
    const uint8_t next = (uint8_t)((previous & ~mask) | (value & mask));
    return next == previous ? ESP_OK : write_codec(reg, next);
}

static esp_err_t write_sequence(const register_value_t *sequence, size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        const esp_err_t error = write_codec(sequence[i].reg, sequence[i].value);
        if (error != ESP_OK) return error;
    }
    return ESP_OK;
}

static esp_err_t add_codec(void)
{
    if (codec != NULL) return ESP_OK;
    const i2c_device_config_t device = {.dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = CODEC_ADDRESS, .scl_speed_hz = CODEC_SCL_SPEED_HZ};
    return i2c_master_bus_add_device(board_bus(), &device, &codec);
}

bool board_tone_try_claim(void)
{
    if (claimed || !audio_arbiter_try_tone(board_audio_arbiter())) return false;
    claimed = true;
    return true;
}

esp_err_t board_tone_cleanup(void)
{
    esp_err_t first = ESP_OK;
    if (codec_touched) {
        bool powered_down = true;
        for (size_t i = 0; i < sizeof(CODEC_STOP) / sizeof(CODEC_STOP[0]); ++i) {
            const esp_err_t error = write_codec(CODEC_STOP[i].reg, CODEC_STOP[i].value);
            if (first == ESP_OK) first = error;
            powered_down = powered_down && error == ESP_OK;
        }
        if (powered_down) codec_touched = false;
    }
    if (transmitter != NULL) {
        if (tx_enabled) {
            const esp_err_t error = i2s_channel_disable(transmitter);
            if (first == ESP_OK) first = error;
            if (error == ESP_OK) tx_enabled = false;
        }
        const esp_err_t error = i2s_del_channel(transmitter);
        if (first == ESP_OK) first = error;
        if (error == ESP_OK) { transmitter = NULL; tx_enabled = false; }
    }
    if (rails_owned) {
        bool codec_powered_off = false;
        const esp_err_t error = board_power_tone_restore(&codec_powered_off);
        if (first == ESP_OK) first = error;
        if (error == ESP_OK) rails_owned = false;
        if (codec_powered_off) codec_touched = false;
    }
    if (claimed && transmitter == NULL && !codec_touched && !rails_owned) {
        audio_arbiter_release(board_audio_arbiter());
        claimed = false;
    }
    return first;
}

static esp_err_t start_transmitter(void)
{
    i2s_chan_config_t channel = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    channel.auto_clear = true;
    esp_err_t error = i2s_new_channel(&channel, &transmitter, NULL);
    if (error != ESP_OK) return error;
    const i2s_std_config_t config = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(BOARD_AUDIO_SAMPLE_RATE_HZ),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {.mclk = GPIO_NUM_19, .bclk = GPIO_NUM_20, .ws = GPIO_NUM_22,
                     .dout = GPIO_NUM_23, .din = I2S_GPIO_UNUSED},
    };
    error = i2s_channel_init_std_mode(transmitter, &config);
    if (error == ESP_OK) error = i2s_channel_enable(transmitter);
    if (error == ESP_OK) tx_enabled = true;
    return error;
}

static esp_err_t configure_codec(void)
{
    codec_touched = true;
    esp_err_t error = write_sequence(CODEC_START, sizeof(CODEC_START) / sizeof(CODEC_START[0]));
    if (error == ESP_OK) error = update_codec(0x06, 0x3f, 0x03);
    if (error == ESP_OK) error = update_codec(0x07, 0x3f, 0x00);
    if (error == ESP_OK) error = update_codec(0x09, 0x5f, 0x0c);
    if (error == ESP_OK) error = update_codec(0x0a, 0x5f, 0x0c);
    if (error == ESP_OK) error = update_codec(0x31, 0x60, 0x00);
    return error;
}

static esp_err_t write_tone(unsigned volume_percent)
{
    for (unsigned frame = 0U; frame < TONE_TOTAL_FRAMES; frame += TONE_FRAMES_PER_BLOCK) {
        int16_t pcm[TONE_FRAMES_PER_BLOCK * 2U];
        for (unsigned i = 0U; i < TONE_FRAMES_PER_BLOCK; ++i) {
            const unsigned position = frame + i;
            int16_t sample = 0;
            if (position < TONE_NOTE_FRAMES || position >= TONE_NOTE_FRAMES + TONE_GAP_FRAMES) {
                const bool second = position >= TONE_NOTE_FRAMES + TONE_GAP_FRAMES;
                const unsigned note = second ? position - TONE_NOTE_FRAMES - TONE_GAP_FRAMES : position;
                const unsigned remaining = TONE_NOTE_FRAMES - 1U - note;
                const unsigned envelope = note < TONE_FADE_FRAMES ? note :
                                          remaining < TONE_FADE_FRAMES ? remaining : TONE_FADE_FRAMES;
                const unsigned frequency = second ? TONE_SECOND_FREQUENCY_HZ : TONE_FIRST_FREQUENCY_HZ;
                const unsigned index = (note * frequency * TONE_WAVE_SIZE / BOARD_AUDIO_SAMPLE_RATE_HZ) % TONE_WAVE_SIZE;
                sample = (int16_t)((int32_t)WAVE[index] * (int32_t)envelope *
                                   (int32_t)volume_percent * TONE_PCM_GAIN /
                                   (TONE_FADE_FRAMES * TONE_MAX_VOLUME_PERCENT));
            }
            pcm[i * 2U] = pcm[i * 2U + 1U] = sample;
        }
        size_t written = 0U;
        const esp_err_t error = i2s_channel_write(transmitter, pcm, sizeof(pcm), &written, TONE_WRITE_TIMEOUT_MS);
        if (error != ESP_OK) return error;
        if (written != sizeof(pcm)) return ESP_ERR_INVALID_SIZE;
    }
    vTaskDelay(pdMS_TO_TICKS(TONE_DRAIN_MS));
    return ESP_OK;
}

esp_err_t board_tone_play(unsigned volume_percent)
{
    if (!claimed || volume_percent > TONE_MAX_VOLUME_PERCENT) return ESP_ERR_INVALID_ARG;
    esp_err_t error = ESP_OK;
    if (volume_percent == 0U) goto done;
    error = add_codec();
    if (error != ESP_OK) goto done;
    rails_owned = true;
    error = board_power_tone_prepare();
    if (error != ESP_OK) goto done;
    vTaskDelay(pdMS_TO_TICKS(TONE_RAIL_SETTLE_MS));
    error = start_transmitter();
    if (error != ESP_OK) goto done;
    uint8_t register_zero = 0U;
    const uint8_t address = 0x00U;
    error = i2c_master_transmit_receive(codec, &address, 1U, &register_zero, 1U, BOARD_I2C_TIMEOUT_MS);
    if (error != ESP_OK) goto done;
    if (!codec_probed) {
        ESP_LOGI(TAG, "ES8311 responds at 0x18, register 0x00=0x%02x", register_zero);
        codec_probed = true;
    }
    error = configure_codec();
    if (error != ESP_OK) goto done;
    error = board_power_tone_amplifier_on();
    if (error != ESP_OK) goto done;
    error = write_tone(volume_percent);
done:
    {
        const esp_err_t cleanup = board_tone_cleanup();
        return error != ESP_OK ? error : cleanup;
    }
}
