#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "bridge_uuid.h"

/* Assistant request contract with the bridge. Times are client budgets, not server guarantees. */
#define BRIDGE_ASSISTANT_ROUTE_PREFIX "/v1/device/assistant-requests/"
#define BRIDGE_ASSISTANT_PATH_SIZE (sizeof(BRIDGE_ASSISTANT_ROUTE_PREFIX) - 1U + BRIDGE_UUID_SIZE)
#define BRIDGE_ASSISTANT_WAV_MIN_BYTES 16044U
#define BRIDGE_ASSISTANT_WAV_MAX_BYTES 960044U
#define BRIDGE_ASSISTANT_UPLOAD_BLOCK_BYTES 2048U
#define BRIDGE_ASSISTANT_JSON_MAX_BYTES 32768U
/* 2000 code points of up to four UTF-8 bytes. */
#define BRIDGE_ASSISTANT_MESSAGE_MAX_BYTES 8000U
#define BRIDGE_ASSISTANT_MESSAGE_SIZE (BRIDGE_ASSISTANT_MESSAGE_MAX_BYTES + 1U)
#define BRIDGE_ASSISTANT_ERROR_CODE_MAX_BYTES 64U

#define BRIDGE_ASSISTANT_PUT_GATE_MS 30000U
#define BRIDGE_ASSISTANT_PUT_OPEN_MS 30000U
/* Each accepted write renews this window, up to 30 s + floor(WAV bytes x 1000 / 4096) ms after opening. */
#define BRIDGE_ASSISTANT_PUT_PROGRESS_MS 30000U
#define BRIDGE_ASSISTANT_PUT_MIN_BYTES_PER_S 4096U
/* The response window starts after the last write and is outside the progress cap. */
#define BRIDGE_ASSISTANT_PUT_RESPONSE_MS 30000U
#define BRIDGE_ASSISTANT_GET_GATE_MS 5000U
#define BRIDGE_ASSISTANT_GET_IO_MS 5000U
/* Local wait after the upload returns or the attempt is restored; it ends with one query. */
#define BRIDGE_ASSISTANT_WAIT_MS 180000U

typedef enum {
    BRIDGE_ASSISTANT_PROCESSING,
    BRIDGE_ASSISTANT_COMPLETED,
    BRIDGE_ASSISTANT_FAILED,
    BRIDGE_ASSISTANT_UNKNOWN,
} bridge_assistant_status_t;

typedef struct {
    bridge_assistant_status_t status;
    /* Only a completed result may omit content. */
    bool content_omitted;
    /* Empty for processing and completed. */
    char error_code[BRIDGE_ASSISTANT_ERROR_CODE_MAX_BYTES + 1U];
} bridge_assistant_result_t;

typedef enum {
    BRIDGE_ASSISTANT_DECODE_OK,
    BRIDGE_ASSISTANT_DECODE_INVALID,
    BRIDGE_ASSISTANT_DECODE_TOO_LARGE,
    /* Any status other than the success ones; it says nothing about remote effects. */
    BRIDGE_ASSISTANT_DECODE_HTTP_ERROR,
} bridge_assistant_decode_t;

/* "/v1/device/assistant-requests/{id}"; false when id is not a canonical UUID or path is too small. */
bool bridge_assistant_path(const char *id, char *path, size_t capacity);

/*
 * Pure decode of the six-field response for expected_id: PUT accepts HTTP 202 processing or HTTP 200 with any status,
 * GET only HTTP 200. Unknown, duplicate or missing fields, another ID or status URL, invalid UTF-8, escaped NUL and
 * a message over the limit are rejected, never truncated. message receives the terminal text (capacity of at least
 * BRIDGE_ASSISTANT_MESSAGE_SIZE); result and message are cleared on any failure.
 */
bridge_assistant_decode_t bridge_assistant_decode(const char *body, size_t length, int http_status, bool upload,
                                                  const char *expected_id, bridge_assistant_result_t *result,
                                                  char *message, size_t capacity);
