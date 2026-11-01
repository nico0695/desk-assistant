#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "connectivity_contract.h"

/* What the UI shows about Wi-Fi: a copy owned by the reader, with no passwords but the temporary setup one. */

typedef enum {
    CONNECTIVITY_NO_NETWORKS,
    CONNECTIVITY_SEARCHING,
    CONNECTIVITY_CONNECTED,
    CONNECTIVITY_OFFLINE,
} connectivity_state_t;

typedef struct {
    connectivity_state_t state;
    uint8_t saved_count;
    char saved[WIFI_LIST_CAPACITY][WIFI_SSID_SIZE];
    /* Index in saved of the connected network; meaningful only when state is CONNECTIVITY_CONNECTED. */
    uint8_t connected_index;
    /* The saved list could not be read or is damaged. */
    bool list_failed;
    bool setup_active;
    char setup_ssid[SETUP_AP_SSID_SIZE];
    char setup_password[SETUP_AP_PASSWORD_SIZE];
    /* The network saved by the setup refused its password; the setup stays open to correct it. */
    bool setup_password_rejected;
} connectivity_view_t;
