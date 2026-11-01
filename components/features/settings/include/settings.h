#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "settings_values.h"

/* Called from the storage task with the result of one save. */
typedef void (*settings_saved_cb_t)(esp_err_t result, uint32_t save_sequence, void *context);

/* Reads every setting; see settings_resolve. Requires storage_init, even if it failed. */
void settings_load(settings_loaded_t *loaded);

/* Queues one save and returns at once. Fails without queuing for an invalid value or unavailable storage. */
esp_err_t settings_save(settings_id_t id, uint8_t value, uint32_t save_sequence, settings_saved_cb_t on_saved,
                        void *context);

/* Saves one value synchronously, for callers outside the UI task (USB provisioning). Fails for an invalid value. */
esp_err_t settings_store(settings_id_t id, uint8_t value);
