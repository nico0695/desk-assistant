#include <string.h>

#include "board.h"
#include "board_battery.h"
#include "bridge.h"
#include "bridge_origin_store.h"
#include "connectivity.h"
#include "esp_log.h"
#include "esp_system.h"
#include "memory_report.h"
#include "nvs.h"
#include "settings.h"
#include "storage.h"
#include "ui.h"

static const char *TAG = "main";

static bool screen_timeout_valid(uint32_t seconds)
{
    return seconds <= UINT8_MAX && settings_value_valid(SETTINGS_SCREEN_TIMEOUT, (uint8_t)seconds);
}

static esp_err_t store_screen_timeout(uint8_t seconds)
{
    const esp_err_t error = settings_store(SETTINGS_SCREEN_TIMEOUT, seconds);
    if (error == ESP_OK) {
        ui_apply_stored_setting(SETTINGS_SCREEN_TIMEOUT, seconds);
    }
    return error;
}

static void show_connectivity(const connectivity_view_t *view)
{
    ui_show_connectivity(view);
    bridge_set_ip(view->state == CONNECTIVITY_CONNECTED);
}

static const connectivity_hooks_t CONNECTIVITY_HOOKS = {
    .screen_timeout_valid = screen_timeout_valid,
    .store_screen_timeout = store_screen_timeout,
    .view_changed = show_connectivity,
};

/* Only whether an origin is usable; the URL and token are never logged. */
static void log_bridge_origin(void)
{
    bridge_origin_t origin;
    const esp_err_t error = bridge_origin_load(&origin);
    memset(&origin, 0, sizeof(origin));
    ESP_LOGI(TAG, "bridge origin %s",
             error == ESP_OK ? "configured" : error == ESP_ERR_NVS_NOT_FOUND ? "not configured" : "unusable");
}

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
    log_bridge_origin();

    const esp_err_t board_error = board_init();
    if (board_error != ESP_OK) {
        ESP_LOGE(TAG, "board init failed: %s", esp_err_to_name(board_error));
    } else {
        ESP_ERROR_CHECK_WITHOUT_ABORT(ui_start(&settings));
        ESP_ERROR_CHECK_WITHOUT_ABORT(board_battery_start(show_battery, NULL));
    }
    ESP_ERROR_CHECK_WITHOUT_ABORT(bridge_start(ui_show_bridge_online));
    ESP_ERROR_CHECK_WITHOUT_ABORT(connectivity_start(&CONNECTIVITY_HOOKS));
    memory_report_log("boot");
}
