#pragma once

#include <stdbool.h>

#include "esp_err.h"

/* One worker claims I2S0 only when no capture is active or about to start. */
bool board_tone_try_claim(void);
/* Blocking ES8311 playback. Requires a claim; always attempts cleanup before returning. */
esp_err_t board_tone_play(unsigned volume_percent);
/* Retry cleanup after any failure; ownership is released only when all resources are safe. */
esp_err_t board_tone_cleanup(void);
