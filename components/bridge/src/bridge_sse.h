#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "bridge_sse_contract.h"
#include "bridge_text.h"
#include "bridge_uuid.h"

/* Owned by the stream task; no pointer escapes the owner. Times are monotonic ms. */
typedef struct {
    bool connected;
    bool invalid;
    bool after_cr;
    bool has_data;
    bool pending_events;
    bool assistant_result;
    uint8_t retry_step;
    bridge_utf8_t utf8;
    uint64_t last_bytes_ms;
    uint64_t disconnected_ms;
    uint64_t retry_delay_ms;
    size_t line_length;
    size_t data_length;
    size_t frame_length;
    char line[BRIDGE_SSE_LINE_MAX_BYTES + 1U];
    char data[BRIDGE_SSE_DATA_MAX_BYTES + 1U];
    char event[BRIDGE_SSE_EVENT_MAX_BYTES + 1U];
    char assistant_id[BRIDGE_UUID_SIZE];
} bridge_sse_t;

void bridge_sse_init(bridge_sse_t *state);
void bridge_sse_connected(bridge_sse_t *state, uint64_t now_ms);
void bridge_sse_disconnected(bridge_sse_t *state, uint64_t now_ms);
bool bridge_sse_retry_due(const bridge_sse_t *state, uint64_t now_ms);
bool bridge_sse_dead(const bridge_sse_t *state, uint64_t now_ms);
/* False requires closing the stream; partial frames are never dispatched. */
bool bridge_sse_feed(bridge_sse_t *state, const void *bytes, size_t length, uint64_t now_ms);
bool bridge_sse_set_assistant(bridge_sse_t *state, const char *uuid);
bool bridge_sse_take_pending_events(bridge_sse_t *state);
bool bridge_sse_take_assistant_result(bridge_sse_t *state);
