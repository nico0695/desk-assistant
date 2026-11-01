#include "board_battery.h"

#include "board_power.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#define BATTERY_TASK_NAME "battery"
#define MS_PER_SECOND 1000U

static const char *TAG = "board_battery";

typedef struct {
    board_battery_cb_t on_reading;
    void *context;
} battery_listener_t;

static battery_listener_t listener;
static TaskHandle_t battery_task;

static void read_periodically(void *unused)
{
    (void)unused;
    const TickType_t period = pdMS_TO_TICKS(CONFIG_BOARD_BATTERY_READ_PERIOD_S * MS_PER_SECOND);
    for (;;) {
        board_battery_reading_t reading;
        const esp_err_t error = board_power_read_battery(&reading);
        if (error != ESP_OK) {
            ESP_LOGW(TAG, "read failed: %s", esp_err_to_name(error));
        }
        listener.on_reading(reading, listener.context);
        vTaskDelay(period);
    }
}

esp_err_t board_battery_start(board_battery_cb_t on_reading, void *context)
{
    ESP_RETURN_ON_FALSE(on_reading != NULL, ESP_ERR_INVALID_ARG, TAG, "callback");
    ESP_RETURN_ON_FALSE(battery_task == NULL, ESP_ERR_INVALID_STATE, TAG, "already started");
    listener = (battery_listener_t){.on_reading = on_reading, .context = context};
    const BaseType_t created = xTaskCreate(read_periodically, BATTERY_TASK_NAME, CONFIG_BOARD_BATTERY_TASK_STACK_SIZE,
                                           NULL, CONFIG_BOARD_BATTERY_TASK_PRIORITY, &battery_task);
    ESP_RETURN_ON_FALSE(created == pdPASS, ESP_ERR_NO_MEM, TAG, "task");
    return ESP_OK;
}
