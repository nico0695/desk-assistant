#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "recording_view.h"

typedef void (*recording_view_fn)(const recording_view_t *view);

/*
 * Starts the attempt owner and the boot cleanup of the audio partition. on_view receives every state change from
 * whichever task produced it and must not block. Requires board_init.
 */
esp_err_t recording_start(recording_view_fn on_view);

/*
 * Commands from the UI: never block on audio or flash. A command whose generation differs from the current one, or
 * that the phase does not allow, has no effect.
 */
void recording_record(uint64_t generation);
void recording_send(uint64_t generation);
void recording_cancel(uint64_t generation);
void recording_close(uint64_t generation);
