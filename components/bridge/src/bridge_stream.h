#pragma once

#include <stdbool.h>
#include "bridge_origin_store.h"
#include "bridge_sse.h"

typedef struct {
    bool (*opened)(void *context);
    bool (*current)(void *context);
    void *context;
} bridge_stream_hooks_t;

typedef enum {
    BRIDGE_STREAM_STOPPED,
    BRIDGE_STREAM_SETUP_FAILED,
    BRIDGE_STREAM_CONNECT_FAILED,
    BRIDGE_STREAM_HEADERS_FAILED,
    BRIDGE_STREAM_HTTP_REJECTED,
    BRIDGE_STREAM_INVALID_HEADERS,
    BRIDGE_STREAM_HEARTBEAT_TIMEOUT,
    BRIDGE_STREAM_READ_FAILED,
    BRIDGE_STREAM_INVALID_FRAME,
} bridge_stream_reason_t;

typedef struct {
    bridge_stream_reason_t reason;
    int http_status;
} bridge_stream_result_t;

/* The caller owns origin and state; this function cleans up all HTTP/TLS resources. */
bridge_stream_result_t bridge_stream_run(const bridge_origin_t *origin, bridge_sse_t *state,
                                         const bridge_stream_hooks_t *hooks);
