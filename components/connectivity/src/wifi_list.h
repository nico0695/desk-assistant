#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "connectivity_contract.h"

/* Passwords stay in the list and in NVS: never copy them to the UI, logs or replies. */
typedef struct {
    char ssid[WIFI_SSID_SIZE];
    char password[WIFI_PASSWORD_SIZE];
} wifi_network_t;

/* Most recently saved first. This is also the stored record: unused slots are zero. */
typedef struct {
    uint8_t count;
    wifi_network_t networks[WIFI_LIST_CAPACITY];
} wifi_list_t;

/* SSID of 1-32 bytes; NUL cannot be represented and is the terminator. */
bool wifi_ssid_valid(const char *ssid);

/* Empty for an explicitly open network; 8-63 printable ASCII; or 64 hexadecimal digits. */
bool wifi_password_valid(const char *password);

/* A stored record: count in range, valid unique entries, zeroed unused slots. */
bool wifi_list_valid(const wifi_list_t *list);

/* Index of the SSID, or -1. */
int wifi_list_find(const wifi_list_t *list, const char *ssid);

typedef enum {
    WIFI_LIST_SAVE_INVALID,
    WIFI_LIST_SAVE_ADDED,
    WIFI_LIST_SAVE_UPDATED,
    WIFI_LIST_SAVE_REPLACED_OLDEST,
} wifi_list_save_t;

/*
 * Puts the network first. The same SSID replaces its password; a new network on a full list drops the oldest,
 * whose SSID is copied to replaced_ssid when given. The list is unchanged when the network is invalid.
 */
wifi_list_save_t wifi_list_save(wifi_list_t *list, const char *ssid, const char *password,
                                char replaced_ssid[WIFI_SSID_SIZE]);

/* Removes the SSID and zeroes the freed slot. False when it is not saved. */
bool wifi_list_forget(wifi_list_t *list, const char *ssid);
