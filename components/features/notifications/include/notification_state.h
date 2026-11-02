#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "bridge_events.h"

typedef struct {
    bridge_event_t cards[BRIDGE_EVENT_QUEUE_CAPACITY];
    uint8_t count;
    char pending_ack[BRIDGE_UUID_SIZE];
} notification_state_t;

typedef enum {
    NOTIFICATION_RECEIVED,
    NOTIFICATION_DUPLICATE,
    NOTIFICATION_FULL,
    NOTIFICATION_ACK_PENDING,
    NOTIFICATION_INVALID,
} notification_receive_t;

void notification_state_init(notification_state_t *state);
bool notification_state_valid(const notification_state_t *state);
bool notification_state_has_work(const notification_state_t *state);
notification_receive_t notification_state_receive(notification_state_t *state, const bridge_event_t *event);
bool notification_state_acknowledged(notification_state_t *state, const char *event_id);
bool notification_state_close(notification_state_t *state, const char *event_id);
