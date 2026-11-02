#include "bridge_clock.h"

#include <stdatomic.h>
#include <sys/time.h>

#include "esp_netif_sntp.h"
#include "sdkconfig.h"

static atomic_bool synchronized;
static bool started;
static bridge_clock_synced_fn synced_callback;
static void *synced_context;

static void time_synchronized(struct timeval *time_value)
{
    (void)time_value;
    atomic_store(&synchronized, true);
    if (synced_callback != NULL) {
        synced_callback(synced_context);
    }
}

esp_err_t bridge_clock_start(bridge_clock_synced_fn on_synced, void *context)
{
    if (started) {
        return ESP_OK;
    }
    synced_callback = on_synced;
    synced_context = context;
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(CONFIG_BRIDGE_SNTP_SERVER);
    config.sync_cb = time_synchronized;
    esp_err_t error = esp_netif_sntp_init(&config);
    if (error == ESP_OK) {
        started = true;
    }
    return error;
}

bool bridge_clock_ready(void)
{
    return atomic_load(&synchronized);
}
