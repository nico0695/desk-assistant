#include "bridge_connection.h"

#include <string.h>

static void invalidate(bridge_connection_t *state)
{
    state->stream_open = false;
    if (state->generation == UINT64_MAX) {
        state->exhausted = true;
    } else {
        ++state->generation;
    }
}

void bridge_connection_init(bridge_connection_t *state)
{
    memset(state, 0, sizeof(*state));
}

void bridge_connection_set_ip(bridge_connection_t *state, bool has_ip)
{
    if (state->has_ip != has_ip) {
        state->has_ip = has_ip;
        invalidate(state);
    }
}

void bridge_connection_origin_changed(bridge_connection_t *state, bool has_origin)
{
    state->has_origin = has_origin;
    invalidate(state);
}

uint64_t bridge_connection_begin_stream(bridge_connection_t *state)
{
    if (!state->has_ip || !state->has_origin || state->exhausted) {
        return 0;
    }
    invalidate(state);
    return state->exhausted ? 0 : state->generation;
}

bool bridge_connection_is_current(const bridge_connection_t *state, uint64_t ticket)
{
    return ticket != 0 && !state->exhausted && state->has_ip && state->has_origin &&
           state->generation == ticket;
}

bool bridge_connection_stream_opened(bridge_connection_t *state, uint64_t ticket)
{
    if (!bridge_connection_is_current(state, ticket)) {
        return false;
    }
    state->stream_open = true;
    return true;
}

bool bridge_connection_stream_closed(bridge_connection_t *state, uint64_t ticket)
{
    if (!bridge_connection_is_current(state, ticket)) {
        return false;
    }
    invalidate(state);
    return true;
}

bool bridge_connection_is_online(const bridge_connection_t *state)
{
    return !state->exhausted && state->has_ip && state->has_origin && state->stream_open;
}
