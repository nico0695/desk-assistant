#pragma once

#include <stdbool.h>

#include "board_battery.h"
#include "esp_err.h"

/* Verifies the AXP2101 identity and pulses ALDO3, which drives the panel reset. Touches only those bits. */
esp_err_t board_power_init(void);

/* Read-only estimate: known only with a valid identity, battery detection enabled, presence and range. */
esp_err_t board_power_read_battery(board_battery_reading_t *reading);

/* Turns ALDO1, the analog supply of the audio codecs, on at 3.3 V after saving its enable bit and voltage. */
esp_err_t board_power_audio_rail_on(void);

/*
 * Restores the saved ALDO1 enable bit and voltage. powered_off reports that the rail is now off, so the codecs on it
 * are unpowered. On failure the saved values stay for a retry.
 */
esp_err_t board_power_audio_rail_restore(bool *powered_off);

/* Prepare ES8311 analog power with PA disabled when originally off, then enable PA after codec setup. */
esp_err_t board_power_tone_prepare(void);
esp_err_t board_power_tone_amplifier_on(void);
/* Restore only ALDO1/ALDO2 bits and voltages changed for the tone; retry after failure. */
esp_err_t board_power_tone_restore(bool *codec_powered_off);
