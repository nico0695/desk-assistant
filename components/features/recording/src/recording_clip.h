#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

/*
 * One temporary PCM16 clip in the audio partition; the WAV header is synthesized on read. Single caller (the
 * recording worker). Audio is never resumed after a reboot. Clearing is operational, not secure erasure.
 */

/* Finds and validates the partition. The clip starts blocked: clear it before the first capture. */
esp_err_t recording_clip_init(void);

/*
 * Erases what may hold audio: the range written since the last clear, or, when that range is unknown (boot, an
 * earlier I/O failure), whatever the scan of the whole partition finds. A clean range is not erased again.
 * On failure the clip stays blocked and the next clear scans again.
 */
esp_err_t recording_clip_clear(void);

/* Requires a cleared clip. */
esp_err_t recording_clip_begin(void);
esp_err_t recording_clip_append(const int16_t *samples, size_t count);
esp_err_t recording_clip_finish(void);

/* Bounded read of the finished WAV by byte offset; EOF returns count 0. An I/O failure blocks the clip. */
esp_err_t recording_clip_read(size_t wav_offset, void *buffer, size_t capacity, size_t *count);
/* Size of the finished WAV, or 0 when the clip is not finished. */
size_t recording_clip_wav_size(void);
