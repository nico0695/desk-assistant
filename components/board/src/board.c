#include "board.h"

#include "board_bus.h"
#include "board_power.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_pm.h"
#include "esp_sleep.h"
#include "sdkconfig.h"

#define I2C_SDA_GPIO GPIO_NUM_8
#define I2C_SCL_GPIO GPIO_NUM_7
#define I2C_GLITCH_FILTER_CYCLES 7

static const char *TAG = "board";
static i2c_master_bus_handle_t shared_bus;

i2c_master_bus_handle_t board_bus(void)
{
    return shared_bus;
}

/* Light sleep with dynamic frequency; whoever needs full speed holds its own lock. KEY and touch wake by GPIO level. */
static esp_err_t configure_power_management(void)
{
#if CONFIG_PM_ENABLE
    ESP_RETURN_ON_ERROR(esp_sleep_enable_gpio_wakeup(), TAG, "GPIO wakeup");
    const esp_pm_config_t config = {
        .max_freq_mhz = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ,
        .min_freq_mhz = CONFIG_BOARD_PM_MIN_CPU_FREQ_MHZ,
        .light_sleep_enable = true,
    };
    return esp_pm_configure(&config);
#else
    return ESP_OK;
#endif
}

esp_err_t board_init(void)
{
    ESP_RETURN_ON_FALSE(shared_bus == NULL, ESP_ERR_INVALID_STATE, TAG, "already initialized");
    const i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = I2C_SDA_GPIO,
        .scl_io_num = I2C_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = I2C_GLITCH_FILTER_CYCLES,
        .flags.enable_internal_pullup = true,
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_config, &shared_bus), TAG, "I2C bus");
    /* A reset in the middle of a transaction can leave a device holding SDA low; clock it free first. */
    const esp_err_t clear_error = i2c_master_bus_reset(shared_bus);
    if (clear_error != ESP_OK) {
        ESP_LOGW(TAG, "I2C bus clear failed: %s", esp_err_to_name(clear_error));
    }
    ESP_RETURN_ON_ERROR(gpio_install_isr_service(0), TAG, "GPIO interrupts");
    /* Not fatal: without it the system simply stays at full speed, as before power management. */
    const esp_err_t power_error = configure_power_management();
    if (power_error != ESP_OK) {
        ESP_LOGW(TAG, "power management: %s", esp_err_to_name(power_error));
    }
    return board_power_init();
}
