#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "bridge_origin_store.h"

/* Receive limits are client budgets, not server guarantees. */
#define BRIDGE_HTTP_HEADER_MAX_BYTES 16384U
#define BRIDGE_HTTP_READ_CHUNK_BYTES 1024U
/* Accommodates one-byte chunks and framing while bounding pathological wire data. */
#define BRIDGE_HTTP_BODY_WIRE_MULTIPLIER 8U
#define BRIDGE_HTTP_EVENT_BODY_MAX_BYTES 4096U
#define BRIDGE_HTTP_BODY_MAX_BYTES 32768U
#define BRIDGE_HTTP_EVENT_IO_MS 5000U
#define BRIDGE_HTTP_EVENT_GATE_MS 5000U

typedef enum {
    BRIDGE_HTTP_OK,
    BRIDGE_HTTP_INVALID_ARGUMENT,
    BRIDGE_HTTP_CLOCK_NOT_READY,
    BRIDGE_HTTP_BUSY,
    BRIDGE_HTTP_NO_MEMORY,
    BRIDGE_HTTP_TRANSPORT_ERROR,
    BRIDGE_HTTP_TIMEOUT,
    BRIDGE_HTTP_INCOMPLETE,
    BRIDGE_HTTP_TOO_LARGE,
    BRIDGE_HTTP_INVALID_RESPONSE,
} bridge_http_result_t;

typedef struct {
    bridge_http_result_t result;
    int status;
    size_t length;
} bridge_http_response_t;

/* Call once during serialized startup, before any HTTP operations. */
esp_err_t bridge_http_init(void);

/* Shared with SSE. Acquire only for connect/handshake; do not hold metadata or UI locks. */
bool bridge_http_handshake_acquire(uint32_t wait_ms);
void bridge_http_handshake_release(void);

/*
 * One GET, never redirected or repeated. Caller owns origin, body and response.
 * path is a trusted relative /v1/device/ route, including an optional query.
 * gate_wait_ms is separate from the socket/TLS io_ms deadline. DNS has its own
 * resolver deadline. HTTP status and complete bounded body are returned for
 * validation by the operation's codec. Body is cleared on failure.
 */
bridge_http_response_t bridge_http_get(const bridge_origin_t *origin, const char *path,
                                       uint32_t gate_wait_ms, uint32_t io_ms,
                                       char *body, size_t capacity);
