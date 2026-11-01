#include "connectivity.h"

#include "bridge_origin_store.h"
#include "esp_check.h"
#include "esp_log.h"
#include "network_store.h"
#include "provision_usb.h"
#include "wifi_station.h"

static const char *TAG = "connectivity";
static connectivity_hooks_t hooks;

static bool save_origin(const char *url, const char *token)
{
    return bridge_origin_save(url, token) == ESP_OK;
}

static bool store_timeout(uint8_t seconds)
{
    return hooks.store_screen_timeout(seconds) == ESP_OK;
}

static bool save_network(const char *ssid, const char *password)
{
    char replaced_ssid[WIFI_SSID_SIZE];
    if (network_store_save(ssid, password, replaced_ssid) != ESP_OK) {
        return false;
    }
    station_network_saved();
    return true;
}

static bool timeout_valid(uint32_t seconds)
{
    return hooks.screen_timeout_valid(seconds);
}

static const provision_actions_t USB_ACTIONS = {
    .timeout_valid = timeout_valid,
    .save_origin = save_origin,
    .store_timeout = store_timeout,
    .save_network = save_network,
};

esp_err_t connectivity_start(const connectivity_hooks_t *connectivity_hooks)
{
    hooks = *connectivity_hooks;
    network_store_load();
    /* A Wi-Fi failure does not stop provisioning over USB. */
    const esp_err_t station_error = station_start(hooks.view_changed);
    if (station_error != ESP_OK) {
        ESP_LOGE(TAG, "station not started: %s", esp_err_to_name(station_error));
    }
    ESP_RETURN_ON_ERROR(provision_usb_start(&USB_ACTIONS), TAG, "USB provisioning not started");
    return ESP_OK;
}

void connectivity_forget(const char *ssid)
{
    station_forget(ssid);
}

void connectivity_open_setup(void)
{
    station_open_setup();
}

void connectivity_cancel_setup(void)
{
    station_cancel_setup();
}
