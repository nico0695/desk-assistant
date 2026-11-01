#pragma once

#include "bridge_origin.h"
#include "esp_err.h"

/* Holds the device token: zero it after use and never log it. */
typedef struct {
    char url[BRIDGE_URL_SIZE];
    char token[BRIDGE_TOKEN_SIZE];
} bridge_origin_t;

/*
 * Replaces the stored origin with one write, so URL and token change together or not at all. This is the single
 * point where the origin changes. ESP_ERR_INVALID_ARG when either value is invalid; nothing is written then.
 */
esp_err_t bridge_origin_save(const char *url, const char *token);

/*
 * ESP_OK with a valid origin; ESP_ERR_NVS_NOT_FOUND when none was configured; any other error (unreadable,
 * incompatible or invalid record) leaves the origin unusable and nothing is erased. The origin is zeroed on error.
 */
esp_err_t bridge_origin_load(bridge_origin_t *origin);
