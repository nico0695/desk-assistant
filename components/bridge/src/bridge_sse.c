#include "bridge_sse.h"

#include <string.h>

#include "bridge_text.h"
#include "cJSON.h"

void bridge_sse_init(bridge_sse_t *state)
{
    memset(state, 0, sizeof(*state));
}

void bridge_sse_connected(bridge_sse_t *state, uint64_t now_ms)
{
    char assistant_id[BRIDGE_UUID_SIZE];
    memcpy(assistant_id, state->assistant_id, sizeof(assistant_id));
    bridge_sse_init(state);
    memcpy(state->assistant_id, assistant_id, sizeof(assistant_id));
    state->connected = true;
    state->pending_events = true;
    state->assistant_result = assistant_id[0] != '\0';
    state->last_bytes_ms = now_ms;
}

void bridge_sse_disconnected(bridge_sse_t *state, uint64_t now_ms)
{
    static const uint64_t delays[] = {
        BRIDGE_SSE_RETRY_FIRST_MS, BRIDGE_SSE_RETRY_SECOND_MS, BRIDGE_SSE_RETRY_MAX_MS,
    };
    state->connected = false;
    state->pending_events = false;
    state->assistant_result = false;
    state->disconnected_ms = now_ms;
    state->retry_delay_ms = delays[state->retry_step];
    if (state->retry_step < 2U) {
        ++state->retry_step;
    }
}

bool bridge_sse_retry_due(const bridge_sse_t *state, uint64_t now_ms)
{
    return !state->connected && now_ms >= state->disconnected_ms &&
           now_ms - state->disconnected_ms >= state->retry_delay_ms;
}

bool bridge_sse_dead(const bridge_sse_t *state, uint64_t now_ms)
{
    return state->connected && now_ms >= state->last_bytes_ms &&
           now_ms - state->last_bytes_ms >= BRIDGE_SSE_DEAD_MS;
}

static bool dispatch_frame(bridge_sse_t *state)
{
    const bool pending = strcmp(state->event, "pending_events") == 0;
    const bool assistant = strcmp(state->event, "assistant_result") == 0;
    if ((!pending && !assistant) || !state->has_data) {
        return true;
    }
    /* One shallow object with at most one member. */
    if (!bridge_json_bounded(state->data, state->data_length, 1U, 1U)) {
        return false;
    }
    const char *end = NULL;
    cJSON *root = cJSON_ParseWithLengthOpts(state->data, state->data_length + 1U, &end, true);
    if (root == NULL) {
        return false;
    }
    bool valid = cJSON_IsObject(root);
    const cJSON *field = root->child;
    if (pending) {
        valid = valid && field == NULL;
        if (valid) {
            state->pending_events = true;
        }
    } else {
        valid = valid && field != NULL && field->next == NULL && field->string != NULL &&
                strcmp(field->string, "request_id") == 0 && cJSON_IsString(field) &&
                bridge_uuid_valid(field->valuestring);
        if (valid && state->assistant_id[0] != '\0' && strcmp(field->valuestring, state->assistant_id) == 0) {
            state->assistant_result = true;
        }
    }
    cJSON_Delete(root);
    return valid;
}

static bool finish_line(bridge_sse_t *state)
{
    state->line[state->line_length] = '\0';
    if (state->line_length == 0U) {
        if (!dispatch_frame(state)) {
            return false;
        }
        state->event[0] = '\0';
        state->data[0] = '\0';
        state->data_length = 0;
        state->has_data = false;
        state->frame_length = 0;
    } else if (state->line[0] != ':') {
        char *value = strchr(state->line, ':');
        if (value != NULL) {
            *value++ = '\0';
            if (*value == ' ') {
                ++value;
            }
        } else {
            value = state->line + state->line_length;
        }
        const size_t length = strlen(value);
        if (strcmp(state->line, "event") == 0) {
            if (length > BRIDGE_SSE_EVENT_MAX_BYTES) {
                return false;
            }
            memcpy(state->event, value, length + 1U);
        } else if (strcmp(state->line, "data") == 0) {
            const size_t separator = state->has_data ? 1U : 0U;
            if (length + separator > BRIDGE_SSE_DATA_MAX_BYTES - state->data_length) {
                return false;
            }
            if (separator != 0U) {
                state->data[state->data_length++] = '\n';
            }
            memcpy(state->data + state->data_length, value, length + 1U);
            state->data_length += length;
            state->has_data = true;
        }
    }
    state->line_length = 0;
    return true;
}

bool bridge_sse_feed(bridge_sse_t *state, const void *bytes, size_t length, uint64_t now_ms)
{
    if (!state->connected || state->invalid || (bytes == NULL && length != 0U)) {
        return false;
    }
    if (length != 0U && now_ms >= state->last_bytes_ms) {
        state->last_bytes_ms = now_ms;
    }
    const unsigned char *input = bytes;
    for (size_t i = 0; i < length; ++i) {
        const unsigned char c = input[i];
        if (c == '\n' && state->after_cr) {
            state->after_cr = false;
            continue;
        }
        state->after_cr = false;
        if (c == 0U || ++state->frame_length > BRIDGE_SSE_FRAME_MAX_BYTES || !bridge_utf8_step(&state->utf8, c)) {
            goto invalid;
        }
        if (c == '\r' || c == '\n') {
            state->after_cr = c == '\r';
            if (!finish_line(state)) {
                goto invalid;
            }
        } else {
            if (state->line_length == BRIDGE_SSE_LINE_MAX_BYTES) {
                goto invalid;
            }
            state->line[state->line_length++] = (char)c;
        }
    }
    return true;
invalid:
    state->invalid = true;
    state->pending_events = false;
    state->assistant_result = false;
    return false;
}

bool bridge_sse_set_assistant(bridge_sse_t *state, const char *uuid)
{
    if (uuid != NULL && !bridge_uuid_valid(uuid)) {
        return false;
    }
    const char *next = uuid != NULL ? uuid : "";
    if (strcmp(state->assistant_id, next) != 0) {
        state->assistant_result = false;
        if (uuid != NULL) {
            memcpy(state->assistant_id, uuid, BRIDGE_UUID_SIZE);
        } else {
            memset(state->assistant_id, 0, sizeof(state->assistant_id));
        }
    }
    return true;
}

bool bridge_sse_take_pending_events(bridge_sse_t *state)
{
    if (!state->connected || state->invalid || !state->pending_events) {
        return false;
    }
    state->pending_events = false;
    return true;
}

bool bridge_sse_take_assistant_result(bridge_sse_t *state)
{
    if (!state->connected || state->invalid || !state->assistant_result) {
        return false;
    }
    state->assistant_result = false;
    return true;
}
