#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "bridge_time.h"
#include "bridge_uuid.h"

#define BRIDGE_EVENT_MESSAGE_MAX_BYTES 512U
#define BRIDGE_EVENT_MESSAGE_SIZE (BRIDGE_EVENT_MESSAGE_MAX_BYTES + 1U)
#define BRIDGE_EVENT_TIME_SIZE BRIDGE_TIME_UTC_SIZE
#define BRIDGE_EVENT_GET_PATH "/v1/device/events?limit=1"
#define BRIDGE_EVENT_RESPONSE_MAX_BYTES 4096U
#define BRIDGE_EVENT_ACK_BODY "{\"status\":\"displayed\"}"
#define BRIDGE_EVENT_ACK_PATH_FORMAT "/v1/device/events/%s/ack"
#define BRIDGE_EVENT_QUEUE_CAPACITY 8U

typedef struct {
    char event_id[BRIDGE_UUID_SIZE];
    uint64_t alert_id;
    char message[BRIDGE_EVENT_MESSAGE_SIZE];
    char scheduled_at[BRIDGE_EVENT_TIME_SIZE];
    char triggered_at[BRIDGE_EVENT_TIME_SIZE];
} bridge_event_t;

typedef enum {
    BRIDGE_EVENT_DECODE_OK,
    BRIDGE_EVENT_DECODE_INVALID,
    BRIDGE_EVENT_DECODE_TOO_LARGE,
} bridge_event_decode_t;

bool bridge_event_valid(const bridge_event_t *event);
/* Exact GET /events?limit=1 response. Failure leaves outputs unchanged. */
bridge_event_decode_t bridge_event_decode(const char *body, size_t length, int status,
                                          bridge_event_t *event, bool *present);
bool bridge_event_ack_succeeded(int status, size_t body_length);
/* A definitive ACK refusal (invalid request or unknown event): the same identity is not retried. */
bool bridge_event_ack_rejected(int status);
