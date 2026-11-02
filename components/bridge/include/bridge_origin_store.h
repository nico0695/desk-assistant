#pragma once

#include <stdbool.h>

#include "bridge_origin.h"
#include "esp_err.h"

/* Holds the device token: zero it after use and never log it. */
typedef struct {
    char url[BRIDGE_URL_SIZE];
    char token[BRIDGE_TOKEN_SIZE];
} bridge_origin_t;

/* Registered during startup before provisioning; callback copies the origin before returning. */
typedef void (*bridge_origin_changed_fn)(const bridge_origin_t *origin);
void bridge_origin_set_changed_callback(bridge_origin_changed_fn callback);

/*
 * Replaces the stored origin with one write, so URL and token change together or not at all. This is the single
 * point where the origin changes. ESP_ERR_INVALID_ARG when either value is invalid and ESP_ERR_INVALID_STATE while
 * the origin is held, unless the values equal the stored origin (ESP_OK); nothing is written then.
 */
esp_err_t bridge_origin_save(const char *url, const char *token);

/*
 * Durable work bound to the current origin (an attempt record, pending acknowledgments, or a record that cannot be
 * read) holds it so it cannot change until that work ends. Holds nest. false while a save is in progress: nothing is
 * held then and the caller may try again.
 */
bool bridge_origin_hold(void);
void bridge_origin_release(void);

/*
 * ESP_OK with a valid origin; ESP_ERR_NVS_NOT_FOUND when none was configured; any other error (unreadable,
 * incompatible or invalid record) leaves the origin unusable and nothing is erased. The origin is zeroed on error.
 */
esp_err_t bridge_origin_load(bridge_origin_t *origin);
