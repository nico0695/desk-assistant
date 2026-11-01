/*
 * CST9220 reset and read sequence derived from Waveshare's esp_lcd_touch_cst9217 driver.
 * SPDX-FileCopyrightText: 2015-2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-FileCopyrightText: 2025 Waveshare
 * SPDX-License-Identifier: Apache-2.0
 * Modified for this project; see THIRD_PARTY_NOTICES.md.
 */
#include "board_touch.h"

#include "board_bus.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "touch_decode.h"

#define TOUCH_ADDRESS 0x5a
#define TOUCH_SCL_SPEED_HZ 400000
#define TOUCH_RESET_GPIO GPIO_NUM_11
#define TOUCH_INTERRUPT_GPIO GPIO_NUM_5
#define TOUCH_I2C_TIMEOUT_MS 20
#define TOUCH_RESET_LOW_MS 10
#define TOUCH_RESET_SETTLE_MS 50
/* The controller needs a pause between selecting the data register and reading it. */
#define TOUCH_READ_SETTLE_MS 2

static const uint8_t DATA_REGISTER[] = {0xd0, 0x00};

static const char *TAG = "board_touch";
static i2c_master_dev_handle_t touch_device;
static board_touch_sample_t last_sample;

static TickType_t at_least_one_tick(uint32_t milliseconds)
{
    const TickType_t ticks = pdMS_TO_TICKS(milliseconds);
    return ticks > 0 ? ticks : 1;
}

static esp_err_t reset_controller(void)
{
    const gpio_config_t reset = {
        .pin_bit_mask = 1ULL << TOUCH_RESET_GPIO,
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&reset), TAG, "reset GPIO");
    ESP_RETURN_ON_ERROR(gpio_set_level(TOUCH_RESET_GPIO, 0), TAG, "reset low");
    vTaskDelay(at_least_one_tick(TOUCH_RESET_LOW_MS));
    ESP_RETURN_ON_ERROR(gpio_set_level(TOUCH_RESET_GPIO, 1), TAG, "reset high");
    vTaskDelay(at_least_one_tick(TOUCH_RESET_SETTLE_MS));
    return ESP_OK;
}

static esp_err_t enable_interrupt(board_isr_cb_t on_interrupt, void *context)
{
    const gpio_config_t interrupt = {
        .pin_bit_mask = 1ULL << TOUCH_INTERRUPT_GPIO,
        .mode = GPIO_MODE_INPUT,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&interrupt), TAG, "interrupt GPIO");
    return gpio_isr_handler_add(TOUCH_INTERRUPT_GPIO, on_interrupt, context);
}

esp_err_t board_touch_init(board_isr_cb_t on_interrupt, void *context)
{
    ESP_RETURN_ON_FALSE(on_interrupt != NULL, ESP_ERR_INVALID_ARG, TAG, "callback");
    ESP_RETURN_ON_FALSE(touch_device == NULL, ESP_ERR_INVALID_STATE, TAG, "already initialized");
    const i2c_device_config_t device = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = TOUCH_ADDRESS,
        .scl_speed_hz = TOUCH_SCL_SPEED_HZ,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(board_bus(), &device, &touch_device), TAG, "add device");
    ESP_RETURN_ON_ERROR(reset_controller(), TAG, "reset");
    ESP_RETURN_ON_ERROR(i2c_master_probe(board_bus(), TOUCH_ADDRESS, TOUCH_I2C_TIMEOUT_MS), TAG, "probe");
    last_sample = (board_touch_sample_t){0};
    return enable_interrupt(on_interrupt, context);
}

esp_err_t board_touch_read(board_touch_sample_t *sample)
{
    ESP_RETURN_ON_FALSE(sample != NULL, ESP_ERR_INVALID_ARG, TAG, "sample");
    ESP_RETURN_ON_FALSE(touch_device != NULL, ESP_ERR_INVALID_STATE, TAG, "not initialized");

    uint8_t packet[TOUCH_PACKET_LENGTH];
    esp_err_t error = i2c_master_transmit(touch_device, DATA_REGISTER, sizeof(DATA_REGISTER), TOUCH_I2C_TIMEOUT_MS);
    if (error == ESP_OK) {
        vTaskDelay(at_least_one_tick(TOUCH_READ_SETTLE_MS));
        error = i2c_master_receive(touch_device, packet, sizeof(packet), TOUCH_I2C_TIMEOUT_MS);
    }
    if (error == ESP_OK && !touch_decode(packet, sizeof(packet), &last_sample)) {
        error = ESP_ERR_INVALID_RESPONSE;
    }
    if (error != ESP_OK) {
        last_sample.pressed = false;
    }
    *sample = last_sample;
    return error;
}
