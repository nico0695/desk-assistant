#include "network_store.h"

#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"
#include "storage.h"

/* A new namespace marks an incompatible record; the old one is left untouched. */
#define NETWORK_NAMESPACE "wifi_v1"
#define NETWORK_LIST_KEY "list"

static const char *TAG = "networks";
static SemaphoreHandle_t lock;
static StaticSemaphore_t lock_storage;
/* Guarded by lock. staging holds the next list while it is written, so a failed write leaves saved unchanged. */
static wifi_list_t saved;
static wifi_list_t staging;
static bool load_failed;

void network_store_load(void)
{
    lock = xSemaphoreCreateMutexStatic(&lock_storage);
    const esp_err_t error = storage_get_blob(NETWORK_NAMESPACE, NETWORK_LIST_KEY, &staging, sizeof(staging));
    if (error == ESP_OK && wifi_list_valid(&staging)) {
        saved = staging;
    } else if (error != ESP_ERR_NVS_NOT_FOUND) {
        load_failed = true;
        ESP_LOGW(TAG, "saved networks not loaded (%s); kept in NVS",
                 error == ESP_OK ? "invalid record" : esp_err_to_name(error));
    }
    memset(&staging, 0, sizeof(staging));
    ESP_LOGI(TAG, "%u saved networks", (unsigned)saved.count);
}

bool network_store_failed(void)
{
    xSemaphoreTake(lock, portMAX_DELAY);
    const bool failed = load_failed;
    xSemaphoreGive(lock);
    return failed;
}

esp_err_t network_store_save(const char *ssid, const char *password, char replaced_ssid[WIFI_SSID_SIZE])
{
    replaced_ssid[0] = '\0';
    xSemaphoreTake(lock, portMAX_DELAY);
    staging = saved;
    esp_err_t error = ESP_ERR_INVALID_ARG;
    if (wifi_list_save(&staging, ssid, password, replaced_ssid) != WIFI_LIST_SAVE_INVALID) {
        error = storage_set_blob(NETWORK_NAMESPACE, NETWORK_LIST_KEY, &staging, sizeof(staging));
        if (error == ESP_OK) {
            saved = staging;
            load_failed = false;
        }
    }
    memset(&staging, 0, sizeof(staging));
    xSemaphoreGive(lock);
    if (error != ESP_OK) {
        replaced_ssid[0] = '\0';
    }
    return error;
}
