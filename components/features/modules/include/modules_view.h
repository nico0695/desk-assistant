#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "bridge_resources.h"

typedef enum {
    MODULES_SCREEN_LIST,
    MODULES_SCREEN_DETAIL,
} modules_screen_t;

typedef enum {
    MODULES_LOADING,
    MODULES_READY,
    MODULES_FAILED,
    /* An action is in flight: every control waits for its result. */
    MODULES_ACTING,
} modules_status_t;

typedef enum {
    MODULES_ACTION_NONE,
    MODULES_ACTION_DELETE,
    MODULES_ACTION_COMPLETE,
    MODULES_ACTION_SNOOZE,
} modules_action_t;

/* Progress or outcome of the last action, kept until the next user command. */
typedef enum {
    MODULES_NOTICE_NONE,
    MODULES_NOTICE_WORKING,
    MODULES_NOTICE_DONE,
    /* Not sent: a capture, an upload or another bridge request was in the way. */
    MODULES_NOTICE_BUSY,
    /* Not sent: no connection, configuration or memory, or the bridge did not dispatch it. */
    MODULES_NOTICE_NOT_SENT,
    MODULES_NOTICE_REJECTED,
    /* The resource is absent or inaccessible: read it again before another action. */
    MODULES_NOTICE_NOT_FOUND,
    /* Its state or time changed: read it again before another action. */
    MODULES_NOTICE_CHANGED,
    /* It may have been applied: read it again before another action. */
    MODULES_NOTICE_UNCONFIRMED,
} modules_notice_t;

/* Why a read did not produce data. Never shown as an empty list or as a deleted resource. */
typedef enum {
    MODULES_ERROR_NONE,
    /* A capture or upload owns the device: the read did not start. */
    MODULES_ERROR_BUSY_AUDIO,
    /* Another finite bridge request kept the gate for too long: the read did not start. */
    MODULES_ERROR_BUSY_BRIDGE,
    /* No usable connection, clock or configuration, or a transport failure or timeout. */
    MODULES_ERROR_NETWORK,
    MODULES_ERROR_UNAUTHORIZED,
    MODULES_ERROR_DENIED,
    MODULES_ERROR_UNAVAILABLE,
    MODULES_ERROR_REJECTED,
    MODULES_ERROR_INVALID,
    MODULES_ERROR_TOO_LARGE,
    /* The detail is absent or inaccessible; it does not prove deletion. */
    MODULES_ERROR_NOT_FOUND,
    MODULES_ERROR_NO_MEMORY,
} modules_error_t;

/*
 * What the UI shows. The ticket changes with every navigation intent; commands carry the ticket of the view they
 * were issued from, and a stale one is ignored. Controls follow status: nothing but Back acts while LOADING, and
 * nothing at all while ACTING.
 */
typedef struct {
    bool open;
    bridge_resource_kind_t kind;
    modules_screen_t screen;
    modules_status_t status;
    modules_error_t error;
    uint64_t ticket;
    /* Grows with every published view: the UI keeps the newest and ignores any older one that arrives late. */
    uint64_t revision;
    /* The list page, kept while its detail is shown. */
    uint64_t page;
    bool has_previous;
    bool has_next;
    uint8_t count;
    /* The action the notice is about. */
    modules_action_t action;
    modules_notice_t notice;
    /* UTC time confirmed by a successful Snooze; empty otherwise. */
    char snoozed_until[BRIDGE_TIME_UTC_SIZE];
    /* Actions the shown detail admits now; all false while it must be read again. */
    bool can_delete;
    bool can_complete;
    bool can_snooze;
} modules_view_t;
