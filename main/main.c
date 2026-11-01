#include "esp_log.h"
#include "esp_system.h"
#include "memory_report.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_LOGI(TAG, "boot complete, reset reason %d", esp_reset_reason());
    memory_report_log("boot");
}
