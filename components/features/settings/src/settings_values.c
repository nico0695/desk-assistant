#include "settings_values.h"

const uint8_t SETTINGS_SCREEN_TIMEOUT_OPTIONS_S[SETTINGS_SCREEN_TIMEOUT_OPTION_COUNT] = {
    15U,
    30U,
    60U,
    SETTINGS_SCREEN_TIMEOUT_ALWAYS_ON_S,
};

static bool timeout_valid(uint8_t seconds)
{
    for (unsigned i = 0; i < SETTINGS_SCREEN_TIMEOUT_OPTION_COUNT; ++i) {
        if (SETTINGS_SCREEN_TIMEOUT_OPTIONS_S[i] == seconds) {
            return true;
        }
    }
    return false;
}

bool settings_value_valid(settings_id_t id, uint8_t value)
{
    switch (id) {
    case SETTINGS_SCREEN_TIMEOUT:
        return timeout_valid(value);
    case SETTINGS_VOLUME:
        /* The minimum is 0, the floor of the type. */
        return value <= SETTINGS_VOLUME_MAX_PERCENT;
    case SETTINGS_BRIGHTNESS:
        return value >= SETTINGS_BRIGHTNESS_MIN_PERCENT && value <= SETTINGS_BRIGHTNESS_MAX_PERCENT;
    default:
        return false;
    }
}

uint8_t settings_default(settings_id_t id)
{
    switch (id) {
    case SETTINGS_SCREEN_TIMEOUT:
        return SETTINGS_SCREEN_TIMEOUT_DEFAULT_S;
    case SETTINGS_VOLUME:
        return SETTINGS_VOLUME_DEFAULT_PERCENT;
    case SETTINGS_BRIGHTNESS:
    default:
        return SETTINGS_BRIGHTNESS_DEFAULT_PERCENT;
    }
}

void settings_resolve(settings_loaded_t *loaded, settings_id_t id, settings_read_t read, uint8_t stored)
{
    const bool usable = read == SETTINGS_READ_OK && settings_value_valid(id, stored);
    loaded->values[id] = usable ? stored : settings_default(id);
    loaded->load_failed[id] = !usable && read != SETTINGS_READ_MISSING;
}
