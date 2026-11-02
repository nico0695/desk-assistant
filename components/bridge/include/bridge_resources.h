#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "bridge_time.h"

/* Client budget for a list or detail body: provisional, not a limit of the remote contract. */
#define BRIDGE_RESOURCE_BODY_MAX_BYTES 4096U
/* Decoded strings are copies of body strings, so they fit in the body plus one terminator each. */
#define BRIDGE_RESOURCE_ARENA_SIZE (BRIDGE_RESOURCE_BODY_MAX_BYTES + 16U)
#define BRIDGE_RESOURCE_PAGE_SIZE 5U
#define BRIDGE_RESOURCE_ID_MAX UINT64_C(9007199254740991)
#define BRIDGE_RESOURCE_PATH_SIZE 64U
/* Wait for the shared finite HTTP gate and socket/TLS deadline of a resource read. */
#define BRIDGE_RESOURCE_GATE_WAIT_MS 5000U
#define BRIDGE_RESOURCE_IO_MS 40000U

/* Same order as the home sections. */
typedef enum {
    BRIDGE_RESOURCE_TASKS,
    BRIDGE_RESOURCE_NOTES,
    BRIDGE_RESOURCE_ALERTS,
    BRIDGE_RESOURCE_KIND_COUNT,
} bridge_resource_kind_t;

typedef enum {
    BRIDGE_TASK_PENDING,
    BRIDGE_TASK_IN_PROGRESS,
    BRIDGE_TASK_COMPLETED,
    BRIDGE_TASK_CANCELED,
} bridge_task_status_t;

typedef enum {
    BRIDGE_RESOURCE_OK,
    /* Wrong status, schema, identity, UTF-8 or an incomplete body. */
    BRIDGE_RESOURCE_INVALID,
    /* The body or a string does not fit the client budget. */
    BRIDGE_RESOURCE_TOO_LARGE,
    /* 404 resource_not_found: absent or inaccessible, not proof of deletion. */
    BRIDGE_RESOURCE_NOT_FOUND,
    /* 401: the device credential was rejected. */
    BRIDGE_RESOURCE_UNAUTHORIZED,
    /* 502 bot_auth_rejected or bot_access_denied. */
    BRIDGE_RESOURCE_DENIED,
    /* The bridge could not read: 500/502/503 with a known code. */
    BRIDGE_RESOURCE_UNAVAILABLE,
    /* A well-formed error the read does not map: invalid_request, route unavailable or an unknown code. */
    BRIDGE_RESOURCE_REJECTED,
} bridge_resource_result_t;

/* Strings point into the arena handed to the decoder and live as long as it does. */
typedef struct {
    uint64_t id;
    /* Note or task title, alert message. */
    const char *text;
    char created_at[BRIDGE_TIME_UTC_SIZE];
    /* Task alert_at or alert scheduled_at; empty when a task has none. Empty for notes. */
    char when_at[BRIDGE_TIME_UTC_SIZE];
} bridge_resource_entry_t;

typedef struct {
    bridge_resource_kind_t kind;
    uint64_t page;
    bool has_previous;
    bool has_next;
    uint8_t count;
    bridge_resource_entry_t entries[BRIDGE_RESOURCE_PAGE_SIZE];
} bridge_resource_page_t;

typedef struct {
    bridge_resource_kind_t kind;
    uint64_t id;
    /* Note or task title, alert message. */
    const char *text;
    /* Notes and tasks only; NULL for alerts. */
    const char *description;
    /* NULL when the tag is null. */
    const char *tag;
    /* Tasks only. */
    bridge_task_status_t task_status;
    char created_at[BRIDGE_TIME_UTC_SIZE];
    /* Notes only; empty otherwise. */
    char updated_at[BRIDGE_TIME_UTC_SIZE];
    /* Task alert_at (empty when null) or alert scheduled_at; empty for notes. */
    char when_at[BRIDGE_TIME_UTC_SIZE];
} bridge_resource_detail_t;

/* GET paths. False for an invalid kind, id or page, or one whose pagination offset is not a safe integer. */
bool bridge_resource_list_path(bridge_resource_kind_t kind, uint64_t page, char *path, size_t capacity);
bool bridge_resource_detail_path(bridge_resource_kind_t kind, uint64_t id, char *path, size_t capacity);

/*
 * Exact list response for the requested page. Strings are copied into arena; the page stays valid with it.
 * On any result but OK the outputs are unspecified and the arena is unused.
 */
bridge_resource_result_t bridge_resource_decode_page(bridge_resource_kind_t kind, uint64_t page, int status,
                                                     const char *body, size_t length, char *arena,
                                                     size_t arena_capacity, bridge_resource_page_t *result);
/* Exact detail response for the requested identity; same ownership as the page. */
bridge_resource_result_t bridge_resource_decode_detail(bridge_resource_kind_t kind, uint64_t id, int status,
                                                       const char *body, size_t length, char *arena,
                                                       size_t arena_capacity, bridge_resource_detail_t *result);
