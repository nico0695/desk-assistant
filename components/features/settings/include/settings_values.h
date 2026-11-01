#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    SETTINGS_SCREEN_TIMEOUT,
    SETTINGS_VOLUME,
    SETTINGS_BRIGHTNESS,
    SETTINGS_COUNT,
} settings_id_t;

#define SETTINGS_SCREEN_TIMEOUT_ALWAYS_ON_S 0U
#define SETTINGS_SCREEN_TIMEOUT_DEFAULT_S 30U
#define SETTINGS_SCREEN_TIMEOUT_OPTION_COUNT 4U
/* Allowed screen timeouts in seconds, in display order; always-on last. */
extern const uint8_t SETTINGS_SCREEN_TIMEOUT_OPTIONS_S[SETTINGS_SCREEN_TIMEOUT_OPTION_COUNT];

#define SETTINGS_VOLUME_MIN_PERCENT 0U
#define SETTINGS_VOLUME_MAX_PERCENT 100U
#define SETTINGS_VOLUME_DEFAULT_PERCENT 25U

#define SETTINGS_BRIGHTNESS_MIN_PERCENT 10U
#define SETTINGS_BRIGHTNESS_MAX_PERCENT 100U
#define SETTINGS_BRIGHTNESS_DEFAULT_PERCENT 70U

typedef enum {
    SETTINGS_READ_OK,
    SETTINGS_READ_MISSING,
    SETTINGS_READ_FAILED,
} settings_read_t;

typedef struct {
    uint8_t values[SETTINGS_COUNT];
    bool load_failed[SETTINGS_COUNT];
} settings_loaded_t;

bool settings_value_valid(settings_id_t id, uint8_t value);
uint8_t settings_default(settings_id_t id);

/*
 * Stores the effective value of one setting after reading it: a missing key takes the default without error;
 * a failed read or an invalid stored value takes the default and marks the load as failed.
 */
void settings_resolve(settings_loaded_t *loaded, settings_id_t id, settings_read_t read, uint8_t stored);
