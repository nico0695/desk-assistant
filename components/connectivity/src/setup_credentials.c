#include "setup_credentials.h"

#include <stdio.h>
#include <string.h>

static const char PASSWORD_ALPHABET[] = "abcdefghjkmnpqrstuvwxyz23456789";
static const char HEX_DIGITS[] = "0123456789abcdef";

void setup_ap_ssid(const uint8_t base_mac[6], char ssid[SETUP_AP_SSID_SIZE])
{
    snprintf(ssid, SETUP_AP_SSID_SIZE, SETUP_AP_SSID_PREFIX "%02X%02X", base_mac[4], base_mac[5]);
}

void setup_ap_password(setup_random_t random, char password[SETUP_AP_PASSWORD_SIZE])
{
    /* The bias of a 32-bit value modulo 31 is below one part in 10^8. */
    const uint32_t symbols = (uint32_t)(sizeof(PASSWORD_ALPHABET) - 1U);
    for (unsigned i = 0; i < SETUP_AP_PASSWORD_LENGTH; ++i) {
        password[i] = PASSWORD_ALPHABET[random() % symbols];
    }
    password[SETUP_AP_PASSWORD_LENGTH] = '\0';
}

void setup_nonce(setup_random_t random, char nonce[SETUP_NONCE_SIZE])
{
    for (unsigned i = 0; i < SETUP_NONCE_LENGTH; ++i) {
        nonce[i] = HEX_DIGITS[random() & 0x0FU];
    }
    nonce[SETUP_NONCE_LENGTH] = '\0';
}
