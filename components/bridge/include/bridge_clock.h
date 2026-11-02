#pragma once

#include <stdbool.h>
#include "esp_err.h"

/* Called when SNTP synchronizes; runs in the network task and must not block. */
typedef void (*bridge_clock_synced_fn)(void *context);

/* Start once after station IP is available. A failed start may be retried. */
esp_err_t bridge_clock_start(bridge_clock_synced_fn on_synced, void *context);
bool bridge_clock_ready(void);
