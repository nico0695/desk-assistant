/* AXP2101 register facts from XPowersLib (MIT) and the X-Powers datasheet; see THIRD_PARTY_NOTICES.md. */
#include "board_power.h"

#include <stddef.h>

#include "board_bus.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define PMIC_ADDRESS 0x34
#define PMIC_SCL_SPEED_HZ 100000
#define PMIC_CHIP_ID 0x4a
#define PMIC_ALDO3_RESET_STEP_MS 100
#define BATTERY_PERCENT_MAX 100

enum {
    REGISTER_STATUS = 0x00,
    REGISTER_CHIP_ID = 0x03,
    REGISTER_BATTERY_DETECT = 0x68,
    REGISTER_LDO_ENABLE = 0x90,
    REGISTER_ALDO3_VOLTAGE = 0x94,
    REGISTER_BATTERY_PERCENT = 0xa4,
};

#define STATUS_BATTERY_PRESENT_MASK (1U << 3)
#define BATTERY_DETECT_ENABLED_MASK 1U
#define LDO_ALDO3_ENABLE_MASK (1U << 2)
#define ALDO3_VOLTAGE_MASK 0x1fU
/* ALDO3 output is 500 mV plus 100 mV per step. */
#define ALDO3_VOLTAGE_3300MV ((3300 - 500) / 100)

static const char *TAG = "board_power";
static i2c_master_dev_handle_t pmic;

static esp_err_t read_register(uint8_t reg, uint8_t *value)
{
    return i2c_master_transmit_receive(pmic, &reg, sizeof(reg), value, sizeof(*value), BOARD_I2C_TIMEOUT_MS);
}

static esp_err_t update_register_bits(uint8_t reg, uint8_t mask, uint8_t value)
{
    uint8_t previous = 0;
    ESP_RETURN_ON_ERROR(read_register(reg, &previous), TAG, "read 0x%02x", reg);
    const uint8_t updated = (uint8_t)((previous & ~mask) | (value & mask));
    if (updated == previous) {
        return ESP_OK;
    }
    const uint8_t frame[] = {reg, updated};
    return i2c_master_transmit(pmic, frame, sizeof(frame), BOARD_I2C_TIMEOUT_MS);
}

static esp_err_t check_identity(void)
{
    uint8_t chip_id = 0;
    ESP_RETURN_ON_ERROR(read_register(REGISTER_CHIP_ID, &chip_id), TAG, "chip id");
    ESP_RETURN_ON_FALSE(chip_id == PMIC_CHIP_ID, ESP_ERR_INVALID_RESPONSE, TAG, "unexpected chip id 0x%02x", chip_id);
    return ESP_OK;
}

static esp_err_t pulse_panel_reset(void)
{
    ESP_RETURN_ON_ERROR(update_register_bits(REGISTER_ALDO3_VOLTAGE, ALDO3_VOLTAGE_MASK, ALDO3_VOLTAGE_3300MV), TAG,
                        "ALDO3 voltage");
    const uint8_t reset_levels[] = {LDO_ALDO3_ENABLE_MASK, 0, LDO_ALDO3_ENABLE_MASK};
    esp_err_t error = ESP_OK;
    for (size_t i = 0; error == ESP_OK && i < sizeof(reset_levels); ++i) {
        error = update_register_bits(REGISTER_LDO_ENABLE, LDO_ALDO3_ENABLE_MASK, reset_levels[i]);
        vTaskDelay(pdMS_TO_TICKS(PMIC_ALDO3_RESET_STEP_MS));
    }
    if (error != ESP_OK) {
        /* Never leave the panel held in reset after a failed pulse. */
        (void)update_register_bits(REGISTER_LDO_ENABLE, LDO_ALDO3_ENABLE_MASK, LDO_ALDO3_ENABLE_MASK);
        ESP_LOGE(TAG, "ALDO3 reset pulse: %s", esp_err_to_name(error));
    }
    return error;
}

esp_err_t board_power_init(void)
{
    ESP_RETURN_ON_FALSE(pmic == NULL, ESP_ERR_INVALID_STATE, TAG, "already initialized");
    const i2c_device_config_t device = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = PMIC_ADDRESS,
        .scl_speed_hz = PMIC_SCL_SPEED_HZ,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(board_bus(), &device, &pmic), TAG, "add device");
    ESP_RETURN_ON_ERROR(check_identity(), TAG, "identity");
    return pulse_panel_reset();
}

esp_err_t board_power_read_battery(board_battery_reading_t *reading)
{
    ESP_RETURN_ON_FALSE(reading != NULL, ESP_ERR_INVALID_ARG, TAG, "reading");
    *reading = (board_battery_reading_t){.known = false};
    ESP_RETURN_ON_FALSE(pmic != NULL, ESP_ERR_INVALID_STATE, TAG, "not initialized");

    uint8_t detect = 0, status = 0, percent = 0;
    ESP_RETURN_ON_ERROR(check_identity(), TAG, "identity");
    ESP_RETURN_ON_ERROR(read_register(REGISTER_BATTERY_DETECT, &detect), TAG, "detect");
    /* With detection disabled the presence bit reads as present, so it cannot be trusted. */
    if ((detect & BATTERY_DETECT_ENABLED_MASK) == 0) {
        return ESP_OK;
    }
    ESP_RETURN_ON_ERROR(read_register(REGISTER_STATUS, &status), TAG, "status");
    if ((status & STATUS_BATTERY_PRESENT_MASK) == 0) {
        return ESP_OK;
    }
    ESP_RETURN_ON_ERROR(read_register(REGISTER_BATTERY_PERCENT, &percent), TAG, "percent");
    if (percent <= BATTERY_PERCENT_MAX) {
        *reading = (board_battery_reading_t){.known = true, .percent = percent};
    }
    return ESP_OK;
}
