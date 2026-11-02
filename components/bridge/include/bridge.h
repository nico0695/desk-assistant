#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

/* Called from bridge or event tasks; must not wait. Newer generations supersede older ones. */
typedef void (*bridge_online_fn)(bool online, uint64_t generation);

/* Start after storage and before connectivity. */
esp_err_t bridge_start(bridge_online_fn online_changed);
/* Nonblocking and safe from the Wi-Fi event loop. */
void bridge_set_ip(bool has_ip);
