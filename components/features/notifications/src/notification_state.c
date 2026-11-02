#include "notification_state.h"

#include <string.h>

void notification_state_init(notification_state_t *state)
{
    memset(state, 0, sizeof(*state));
}

static bool empty_tail(const notification_state_t *state)
{
    const unsigned char *tail = (const unsigned char *)&state->cards[state->count];
    for (size_t i = 0; i < (BRIDGE_EVENT_QUEUE_CAPACITY - state->count) * sizeof(state->cards[0]); ++i) {
        if (tail[i] != 0U) return false;
    }
    return true;
}

bool notification_state_valid(const notification_state_t *state)
{
    if (state == NULL || state->count > BRIDGE_EVENT_QUEUE_CAPACITY || !empty_tail(state)) return false;
    if (state->pending_ack[0] != '\0' && !bridge_uuid_valid(state->pending_ack)) return false;
    for (size_t i = 0; i < state->count; ++i) {
        if (!bridge_event_valid(&state->cards[i])) return false;
        for (size_t j = 0; j < i; ++j) {
            if (strcmp(state->cards[i].event_id, state->cards[j].event_id) == 0) return false;
        }
    }
    return true;
}

bool notification_state_has_work(const notification_state_t *state)
{
    return state != NULL && (state->count != 0U || state->pending_ack[0] != '\0');
}

notification_receive_t notification_state_receive(notification_state_t *state, const bridge_event_t *event)
{
    if (state == NULL || !bridge_event_valid(event) || !notification_state_valid(state)) {
        return NOTIFICATION_INVALID;
    }
    if (strcmp(state->pending_ack, event->event_id) == 0) return NOTIFICATION_DUPLICATE;
    for (size_t i = 0; i < state->count; ++i) {
        if (strcmp(state->cards[i].event_id, event->event_id) == 0) {
            if (state->pending_ack[0] == '\0') memcpy(state->pending_ack, event->event_id, BRIDGE_UUID_SIZE);
            return NOTIFICATION_DUPLICATE;
        }
    }
    if (state->pending_ack[0] != '\0') return NOTIFICATION_ACK_PENDING;
    if (state->count == BRIDGE_EVENT_QUEUE_CAPACITY) return NOTIFICATION_FULL;
    state->cards[state->count++] = *event;
    memcpy(state->pending_ack, event->event_id, BRIDGE_UUID_SIZE);
    return NOTIFICATION_RECEIVED;
}

bool notification_state_acknowledged(notification_state_t *state, const char *event_id)
{
    if (state == NULL || !bridge_uuid_valid(event_id) || strcmp(state->pending_ack, event_id) != 0) return false;
    memset(state->pending_ack, 0, sizeof(state->pending_ack));
    return true;
}

bool notification_state_close(notification_state_t *state, const char *event_id)
{
    if (state == NULL || state->count == 0U || !bridge_uuid_valid(event_id) ||
        strcmp(state->cards[0].event_id, event_id) != 0) return false;
    --state->count;
    memmove(&state->cards[0], &state->cards[1], state->count * sizeof(state->cards[0]));
    memset(&state->cards[state->count], 0, sizeof(state->cards[0]));
    return true;
}
