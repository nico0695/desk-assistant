#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "connectivity_contract.h"
#include "connectivity_view.h"
#include "esp_err.h"

/* Effects owned by other components, supplied by main. The timeout hooks are called from the USB task. */
typedef struct {
    bool (*screen_timeout_valid)(uint32_t seconds);
    /* Stores the screen timeout and applies it; ESP_OK when stored. */
    esp_err_t (*store_screen_timeout)(uint8_t seconds);
    /* Called from the event loop task when the Wi-Fi view changed; the view is only valid during the call. Never blocks. */
    void (*view_changed)(const connectivity_view_t *view);
} connectivity_hooks_t;

/*
 * Loads the saved networks and starts USB provisioning. A damaged or unreadable list is reported and kept in NVS
 * until an explicit save replaces it. Requires storage_init, even if it failed.
 */
esp_err_t connectivity_start(const connectivity_hooks_t *hooks);

/* Commands from the UI. They only post a request and never wait; the result arrives through view_changed. */

/* Forgets the saved network by name; unknown names are ignored. */
void connectivity_forget(const char *ssid);

/* Opens the phone setup (access point and page); no effect if it is already open. */
void connectivity_open_setup(void);

/* Closes the phone setup and resumes the normal selection. */
void connectivity_cancel_setup(void);
