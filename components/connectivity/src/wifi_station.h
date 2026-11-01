#pragma once

#include "connectivity_view.h"
#include "esp_err.h"

/* Called on the event loop task with a view that is only valid during the call. */
typedef void (*station_view_changed_t)(const connectivity_view_t *view);

/*
 * Starts Wi-Fi in station mode with credentials kept in RAM only and runs the selection policy and the phone setup
 * on the default event loop. Requires network_store_load. Without saved networks the setup opens by itself, unless
 * the saved list could not be read.
 */
esp_err_t station_start(station_view_changed_t view_changed);

/* A network was just saved first in the list: try it before any other. Safe from any task. */
void station_network_saved(void);

/* Commands that never wait: they are handled on the event loop. Safe from any task. */
void station_forget(const char *ssid);
void station_open_setup(void);
void station_cancel_setup(void);
