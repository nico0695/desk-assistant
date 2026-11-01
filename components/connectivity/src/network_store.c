#include "network_store.h"

#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"
#include "storage.h"
#include "wifi_select.h"

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

esp_err_t network_store_forget(const char *ssid)
{
    xSemaphoreTake(lock, portMAX_DELAY);
    staging = saved;
    esp_err_t error = ESP_ERR_NOT_FOUND;
    if (wifi_list_forget(&staging, ssid)) {
        error = storage_set_blob(NETWORK_NAMESPACE, NETWORK_LIST_KEY, &staging, sizeof(staging));
        if (error == ESP_OK) {
            saved = staging;
            load_failed = false;
        }
    }
    memset(&staging, 0, sizeof(staging));
    xSemaphoreGive(lock);
    return error;
}

void network_store_fill_view(connectivity_view_t *view)
{
    xSemaphoreTake(lock, portMAX_DELAY);
    view->saved_count = saved.count;
    for (unsigned i = 0; i < saved.count; ++i) {
        memcpy(view->saved[i], saved.networks[i].ssid, sizeof(view->saved[i]));
    }
    view->list_failed = load_failed;
    xSemaphoreGive(lock);
}

uint8_t network_store_count(void)
{
    xSemaphoreTake(lock, portMAX_DELAY);
    const uint8_t count = saved.count;
    xSemaphoreGive(lock);
    return count;
}

int network_store_find(const char *ssid)
{
    xSemaphoreTake(lock, portMAX_DELAY);
    const int index = wifi_list_find(&saved, ssid);
    xSemaphoreGive(lock);
    return index;
}

void network_store_note_scan(const char *ssid, int rssi, int16_t best[WIFI_LIST_CAPACITY])
{
    xSemaphoreTake(lock, portMAX_DELAY);
    wifi_select_note(&saved, ssid, rssi, best);
    xSemaphoreGive(lock);
}

bool network_store_station_config(uint8_t index, wifi_config_t *config)
{
    memset(config, 0, sizeof(*config));
    xSemaphoreTake(lock, portMAX_DELAY);
    const bool found = index < saved.count;
    if (found) {
        const wifi_network_t *network = &saved.networks[index];
        const size_t password_length = strlen(network->password);
        memcpy(config->sta.ssid, network->ssid, strlen(network->ssid));
        memcpy(config->sta.password, network->password, password_length);
        /* An open network is only joined when it was saved without a password. */
        config->sta.threshold.authmode = password_length == 0 ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;
        config->sta.pmf_cfg.capable = true;
        config->sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;
    }
    xSemaphoreGive(lock);
    return found;
}
