#pragma once

#include "esp_err.h"

/*
 * Starts Wi-Fi in station mode with credentials kept in RAM only and runs the selection policy on the default event
 * loop. Requires network_store_load.
 */
esp_err_t station_start(void);

/* A network was just saved first in the list: try it before any other. Safe from any task. */
void station_network_saved(void);
