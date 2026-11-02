#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "recording_view.h"

typedef void (*recording_view_fn)(const recording_view_t *view);

/*
 * Starts the attempt owner, restores a stored attempt and runs the boot cleanup of the audio partition. on_view
 * receives every state change from whichever task produced it and must not block. Requires board_init and storage;
 * call before bridge_start.
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
void recording_check(uint64_t generation);
/* The user confirmed discarding the uncertain attempt shown with this generation. */
void recording_discard(uint64_t generation);

/* Bridge signals, from the bridge or event tasks; never block. Newer generations supersede older ones. */
void recording_bridge_online(bool online, uint64_t generation);
void recording_assistant_result(void);

/*
 * Passes the bridge message of the attempt shown with this generation to reader, which must copy it before returning
 * and must not call back into recording. False when there is no message for that generation. Never blocks on I/O.
 */
bool recording_read_message(uint64_t generation, void (*reader)(const char *text, void *context), void *context);

/* Reserve the user action slot for a finite external mutation. Capture and upload commands are blocked until end. */
bool recording_external_action_begin(void);
void recording_external_action_end(void);
