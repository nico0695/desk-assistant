#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "wifi_list.h"

/*
 * Owner of the saved networks in RAM and in NVS, safe from any task. Passwords never leave this module except to
 * the station driver.
 */

/* Loads the stored list once at start. A damaged record leaves the list empty and reported; nothing is erased. */
void network_store_load(void);

/* True while the stored list could not be read and no save has replaced it. */
bool network_store_failed(void);

/*
 * Saves the network first in the list and persists the whole list with one write before changing the RAM copy.
 * replaced_ssid receives the network dropped from a full list, or an empty string.
 */
esp_err_t network_store_save(const char *ssid, const char *password, char replaced_ssid[WIFI_SSID_SIZE]);
