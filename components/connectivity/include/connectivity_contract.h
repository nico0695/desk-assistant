#pragma once

/* Saved networks. */
#define WIFI_LIST_CAPACITY 5U
#define WIFI_SSID_MAX_BYTES 32U
#define WIFI_SSID_SIZE (WIFI_SSID_MAX_BYTES + 1U)
#define WIFI_PASSWORD_MIN_BYTES 8U
#define WIFI_PASSWORD_MAX_TEXT_BYTES 63U
/* A 64-character password is the raw key in hexadecimal. */
#define WIFI_PASSWORD_HEX_KEY_BYTES 64U
#define WIFI_PASSWORD_SIZE (WIFI_PASSWORD_HEX_KEY_BYTES + 1U)

/* Station selection. */
#define WIFI_SELECT_ATTEMPT_MS 15000U
#define WIFI_SELECT_LOSS_WINDOW_MS 30000U
#define WIFI_SELECT_LOSS_RETRY_MS 5000U
#define WIFI_SELECT_ROUND_WAIT_MS 30000U

/* Setup access point. */
#define SETUP_AP_SSID_PREFIX "Desk-Assistant-"
#define SETUP_AP_SSID_SIZE (sizeof(SETUP_AP_SSID_PREFIX) + 4U)
#define SETUP_AP_PASSWORD_LENGTH 8U
#define SETUP_AP_PASSWORD_SIZE (SETUP_AP_PASSWORD_LENGTH + 1U)
#define SETUP_NONCE_LENGTH 32U
#define SETUP_NONCE_SIZE (SETUP_NONCE_LENGTH + 1U)
#define SETUP_URL "http://192.168.4.1"
#define SETUP_AP_CHANNEL 1U
#define SETUP_AP_MAX_CLIENTS 2U

/* USB provisioning: one JSON line per request over USB Serial/JTAG. */
#define PROVISION_PROTOCOL_VERSION 1U
#define PROVISION_LINE_MAX_BYTES 3072U
#define PROVISION_LINE_IDLE_MS 10000U

/* Setup form: application/x-www-form-urlencoded body. */
#define SETUP_FORM_MAX_BYTES 1024U
