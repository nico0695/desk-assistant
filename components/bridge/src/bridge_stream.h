#pragma once

#include <stdbool.h>
#include "bridge_origin_store.h"
#include "bridge_sse.h"

typedef struct {
    bool (*opened)(void *context);
    bool (*current)(void *context);
    void *context;
} bridge_stream_hooks_t;

/* The caller owns origin and state; this function cleans up all HTTP/TLS resources. */
void bridge_stream_run(const bridge_origin_t *origin, bridge_sse_t *state, const bridge_stream_hooks_t *hooks);
