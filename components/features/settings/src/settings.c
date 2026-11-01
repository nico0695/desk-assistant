#include "settings.h"

#include "esp_log.h"
#include "nvs.h"
#include "storage.h"

/* A new namespace marks an incompatible schema; the old one is left untouched. */
#define SETTINGS_NAMESPACE "settings_v1"

static const char *TAG = "settings";

static const char *const KEYS[SETTINGS_COUNT] = {
    [SETTINGS_SCREEN_TIMEOUT] = "timeout_s",
    [SETTINGS_VOLUME] = "volume",
    [SETTINGS_BRIGHTNESS] = "brightness",
};

static settings_read_t classify_read(esp_err_t error)
{
    if (error == ESP_OK) {
        return SETTINGS_READ_OK;
    }
    return error == ESP_ERR_NVS_NOT_FOUND ? SETTINGS_READ_MISSING : SETTINGS_READ_FAILED;
}

void settings_load(settings_loaded_t *loaded)
{
    for (settings_id_t id = 0; id < SETTINGS_COUNT; ++id) {
        uint8_t stored = 0;
        const esp_err_t error = storage_get_u8(SETTINGS_NAMESPACE, KEYS[id], &stored);
        settings_resolve(loaded, id, classify_read(error), stored);
        if (loaded->load_failed[id]) {
            ESP_LOGW(TAG, "%s not loaded (%s), using the default", KEYS[id],
                     error == ESP_OK ? "invalid value" : esp_err_to_name(error));
        }
    }
}

esp_err_t settings_save(settings_id_t id, uint8_t value, uint32_t save_sequence, settings_saved_cb_t on_saved,
                        void *context)
{
    if (id >= SETTINGS_COUNT || !settings_value_valid(id, value)) {
        return ESP_ERR_INVALID_ARG;
    }
    return storage_set_u8_async(SETTINGS_NAMESPACE, KEYS[id], value, save_sequence, on_saved, context);
}
