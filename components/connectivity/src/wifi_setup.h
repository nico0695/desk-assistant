#pragma once

#include <stdbool.h>
#include "connectivity_view.h"
#include "esp_err.h"

/*
 * Phone setup: a temporary access point and an HTTP server that serves the form. Not thread safe: open, close and
 * the readers run on the event loop; the server task only touches the session while it is open.
 */

/* Called from the server task after a network was saved and the response was sent. */
typedef void (*wifi_setup_saved_t)(void);

/* Opens the access point and the server with a new password and nonce. ESP_ERR_INVALID_STATE if already open. */
esp_err_t wifi_setup_open(wifi_setup_saved_t on_saved);

/* Stops the server and the access point and zeroes the session. */
void wifi_setup_close(void);

bool wifi_setup_active(void);

/* Sets setup_active and, while open, the access point name and password. */
void wifi_setup_fill_view(connectivity_view_t *view);
