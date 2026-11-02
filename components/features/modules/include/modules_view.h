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
} modules_status_t;

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
 * were issued from, and a stale one is ignored. Controls follow status: nothing but Back acts while LOADING.
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
} modules_view_t;
