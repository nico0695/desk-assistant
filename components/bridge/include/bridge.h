#pragma once

#include <stdbool.h>
#include "esp_err.h"

/* Called from the bridge task; must not wait. */
typedef void (*bridge_online_fn)(bool online);

/* Start after storage and before connectivity. */
esp_err_t bridge_start(bridge_online_fn online_changed);
/* Nonblocking and safe from the Wi-Fi event loop. */
void bridge_set_ip(bool has_ip);
