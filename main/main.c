#include "board.h"
#include "board_battery.h"
#include "esp_log.h"
#include "esp_system.h"
#include "memory_report.h"
#include "settings.h"
#include "storage.h"
#include "ui.h"

static const char *TAG = "main";

static void show_battery(board_battery_reading_t reading, void *unused)
{
    (void)unused;
    ui_show_battery(reading.known, reading.percent);
}

void app_main(void)
{
    ESP_LOGI(TAG, "reset reason %d", esp_reset_reason());
    /* Without storage the device still runs on defaults; settings_load reports the failure per setting. */
    ESP_ERROR_CHECK_WITHOUT_ABORT(storage_init());
    settings_loaded_t settings;
    settings_load(&settings);

    const esp_err_t board_error = board_init();
    if (board_error != ESP_OK) {
        ESP_LOGE(TAG, "board init failed: %s", esp_err_to_name(board_error));
    } else {
        ESP_ERROR_CHECK_WITHOUT_ABORT(ui_start(&settings));
        ESP_ERROR_CHECK_WITHOUT_ABORT(board_battery_start(show_battery, NULL));
    }
    memory_report_log("boot");
}
