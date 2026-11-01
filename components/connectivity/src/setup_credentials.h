#pragma once

#include <stdint.h>
#include "connectivity_contract.h"

/* Returns 32 random bits; esp_random on the device. */
typedef uint32_t (*setup_random_t)(void);

/* SETUP_AP_SSID_PREFIX plus the last two bytes of the base MAC in uppercase hexadecimal. */
void setup_ap_ssid(const uint8_t base_mac[6], char ssid[SETUP_AP_SSID_SIZE]);

/* Lowercase letters and digits without 0, o, 1, l and i, so it is easy to type on a phone. */
void setup_ap_password(setup_random_t random, char password[SETUP_AP_PASSWORD_SIZE]);

/* Lowercase hexadecimal. */
void setup_nonce(setup_random_t random, char nonce[SETUP_NONCE_SIZE]);
