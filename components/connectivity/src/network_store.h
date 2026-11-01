#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "esp_wifi_types.h"
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

uint8_t network_store_count(void);

/* Index of the SSID in the saved order, or -1. */
int network_store_find(const char *ssid);

/* Records one scanned network for wifi_select_round. */
void network_store_note_scan(const char *ssid, int rssi, int16_t best[WIFI_LIST_CAPACITY]);

/* Fills the station credentials of the network at index; the caller zeroes them after use. False if absent. */
bool network_store_station_config(uint8_t index, wifi_config_t *config);
