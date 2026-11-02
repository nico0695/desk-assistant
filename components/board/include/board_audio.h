#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

/* The ES7210 sequence and the I2S clocks are set up for this rate only. */
#define BOARD_AUDIO_SAMPLE_RATE_HZ 16000U
/* Samples per DMA frame and per read. */
#define BOARD_AUDIO_BLOCK_SAMPLES 256U

/*
 * Microphone capture: ES7210 MIC1 as PCM16 mono over I2S0 RX. One owner calls every function; a second start is
 * rejected. Requires board_init.
 */

/* Powers the audio rail, configures the ES7210 and starts I2S. On failure everything started is released. */
esp_err_t board_audio_start(void);

/* Blocks until up to capacity samples arrive (at most BOARD_AUDIO_BLOCK_SAMPLES) or the timeout ends. */
esp_err_t board_audio_read(int16_t *samples, size_t capacity, size_t *count, uint32_t timeout_ms);

/*
 * Stops I2S, powers the ES7210 down and restores only the rail bits start changed. Continues after a failure and
 * returns the first one; whatever could not be released stays owned so a later stop retries it.
 */
esp_err_t board_audio_stop(void);

/* DMA receive-queue overruns since the last start: at least one block of samples was lost. */
uint32_t board_audio_overruns(void);
