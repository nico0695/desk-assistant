#include "connectivity.h"

#include "bridge_origin_store.h"
#include "esp_check.h"
#include "network_store.h"
#include "provision_usb.h"

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
    return network_store_save(ssid, password, replaced_ssid) == ESP_OK;
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
    ESP_RETURN_ON_ERROR(provision_usb_start(&USB_ACTIONS), TAG, "USB provisioning not started");
    return ESP_OK;
}
