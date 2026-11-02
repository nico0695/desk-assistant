#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "bridge_events.h"

typedef enum {
    NOTIFICATION_VIEW_OK,
    NOTIFICATION_VIEW_STORAGE_ERROR,
    NOTIFICATION_VIEW_SNOOZE_REJECTED,
    NOTIFICATION_VIEW_SNOOZE_UNCONFIRMED,
    NOTIFICATION_VIEW_SNOOZE_CONFIRMED,
} notification_view_notice_t;

typedef struct {
    bridge_event_t card;
    uint8_t count;
    uint64_t generation;
    uint64_t arrival_sequence;
    bool action_busy;
    notification_view_notice_t notice;
    /* Already formatted for the user; the owner retains the complete UTC value for the bridge. */
    char snoozed_at_text[BRIDGE_EVENT_TIME_SIZE];
} notification_view_t;
