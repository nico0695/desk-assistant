#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "bridge_events.h"

#define BRIDGE_SNOOZE_PATH_FORMAT "/v1/device/alerts/%llu/snooze"
#define BRIDGE_SNOOZE_PATH_SIZE 64U
#define BRIDGE_SNOOZE_REQUEST_SIZE 128U

typedef enum {
    BRIDGE_SNOOZE_FIVE_MINUTES = 1,
    BRIDGE_SNOOZE_ONE_HOUR = 2,
    BRIDGE_SNOOZE_TOMORROW = 3,
} bridge_snooze_preset_t;

typedef enum {
    BRIDGE_SNOOZE_SUCCESS,
    BRIDGE_SNOOZE_REJECTED,
    BRIDGE_SNOOZE_NOT_FOUND,
    BRIDGE_SNOOZE_SCHEDULE_CHANGED,
    BRIDGE_SNOOZE_NOT_SENT,
    BRIDGE_SNOOZE_UNCONFIRMED,
} bridge_snooze_result_t;

bool bridge_snooze_path(uint64_t alert_id, char *path, size_t capacity);
bool bridge_snooze_request(bridge_snooze_preset_t preset, const char *scheduled_at,
                           char *body, size_t capacity);
/* A complete response after a dispatched POST. On success, confirmed_at receives the new UTC time. */
bridge_snooze_result_t bridge_snooze_decode(int status, const char *body, size_t length,
                                            uint64_t alert_id, char confirmed_at[BRIDGE_EVENT_TIME_SIZE]);
