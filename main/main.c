#include "board.h"
#include "board_battery.h"
#include "esp_log.h"
#include "esp_system.h"
#include "memory_report.h"

static const char *TAG = "main";

static void log_battery(board_battery_reading_t reading, void *unused)
{
    (void)unused;
    if (reading.known) {
        ESP_LOGI(TAG, "battery %u%%", (unsigned)reading.percent);
    } else {
        ESP_LOGI(TAG, "battery unknown");
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "reset reason %d", esp_reset_reason());
    const esp_err_t board_error = board_init();
    if (board_error != ESP_OK) {
        ESP_LOGE(TAG, "board init failed: %s", esp_err_to_name(board_error));
    } else {
        ESP_ERROR_CHECK_WITHOUT_ABORT(board_battery_start(log_battery, NULL));
    }
    memory_report_log("boot");
}
