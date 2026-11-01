#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "connectivity_contract.h"
#include "esp_err.h"

/* Effects owned by other components, supplied by main. Called from the USB task. */
typedef struct {
    bool (*screen_timeout_valid)(uint32_t seconds);
    /* Stores the screen timeout and applies it; ESP_OK when stored. */
    esp_err_t (*store_screen_timeout)(uint8_t seconds);
} connectivity_hooks_t;

/*
 * Loads the saved networks and starts USB provisioning. A damaged or unreadable list is reported and kept in NVS
 * until an explicit save replaces it. Requires storage_init, even if it failed.
 */
esp_err_t connectivity_start(const connectivity_hooks_t *hooks);
