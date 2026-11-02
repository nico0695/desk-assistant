#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Single-owner state. Synchronize access when events arrive from different tasks. */
typedef struct {
    uint64_t generation;
    bool exhausted;
    bool has_ip;
    bool has_origin;
    bool stream_open;
} bridge_connection_t;

void bridge_connection_init(bridge_connection_t *state);
void bridge_connection_set_ip(bridge_connection_t *state, bool has_ip);
/* Called after each successful origin replacement, including replacements with another configured origin. */
void bridge_connection_origin_changed(bridge_connection_t *state, bool has_origin);
/* Returns a new nonzero ticket, or zero when a stream cannot be started. */
uint64_t bridge_connection_begin_stream(bridge_connection_t *state);
/* Old tickets never change the current stream or its presentation. */
bool bridge_connection_stream_opened(bridge_connection_t *state, uint64_t ticket);
bool bridge_connection_stream_closed(bridge_connection_t *state, uint64_t ticket);
bool bridge_connection_is_current(const bridge_connection_t *state, uint64_t ticket);
bool bridge_connection_is_online(const bridge_connection_t *state);
