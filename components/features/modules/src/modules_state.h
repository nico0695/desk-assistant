#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "modules_view.h"

typedef enum {
    MODULES_REQUEST_NONE,
    MODULES_REQUEST_LIST,
    MODULES_REQUEST_DETAIL,
} modules_request_type_t;

typedef struct {
    modules_request_type_t type;
    bridge_resource_kind_t kind;
    /* Page number for a list, resource ID for a detail. */
    uint64_t number;
    uint64_t ticket;
} modules_request_t;

typedef struct {
    bool open;
    bool running;
    bridge_resource_kind_t kind;
    modules_screen_t screen;
    modules_status_t status;
    modules_error_t error;
    uint64_t ticket;
    uint64_t page;
    bool has_previous;
    bool has_next;
    uint8_t count;
    uint64_t ids[BRIDGE_RESOURCE_PAGE_SIZE];
    /* The list stays in memory while its detail is shown; Back uses it once. */
    bool retained;
    uint64_t detail_id;
    modules_request_t wanted;
    modules_request_t running_request;
} modules_state_t;

/* What the owner must do after a transition; combine with bitwise or. */
typedef enum {
    MODULES_EFFECT_NONE = 0,
    MODULES_EFFECT_VIEW = 1 << 0,
    /* A request is wanted: claim it if no worker is running. */
    MODULES_EFFECT_WORK = 1 << 1,
    MODULES_EFFECT_FREE_LIST = 1 << 2,
    MODULES_EFFECT_FREE_DETAIL = 1 << 3,
    /* A finished read still matches the view: install its data. */
    MODULES_EFFECT_ACCEPT = 1 << 4,
    /* A finished read no longer matches the view: free its data. */
    MODULES_EFFECT_DISCARD = 1 << 5,
} modules_effect_t;

typedef struct {
    uint64_t page;
    bool has_previous;
    bool has_next;
    uint8_t count;
    uint64_t ids[BRIDGE_RESOURCE_PAGE_SIZE];
} modules_page_t;

void modules_state_init(modules_state_t *state);
void modules_state_view(const modules_state_t *state, modules_view_t *view);
bool modules_state_is_current(const modules_state_t *state, uint64_t ticket);

/* Opens a module on page 1, discarding anything previous. */
unsigned modules_state_enter(modules_state_t *state, bridge_resource_kind_t kind);
/* Leaves the module: frees everything and drops any wanted request. */
unsigned modules_state_leave(modules_state_t *state);

unsigned modules_state_previous(modules_state_t *state, uint64_t ticket);
unsigned modules_state_next(modules_state_t *state, uint64_t ticket);
/* Rereads the current page, or the current detail. */
unsigned modules_state_refresh(modules_state_t *state, uint64_t ticket);
/* The id must belong to the shown page. Keeps the page for Back. */
unsigned modules_state_open_detail(modules_state_t *state, uint64_t ticket, uint64_t id);
/* From a detail: the retained page once, or a fresh read when it no longer exists. */
unsigned modules_state_back(modules_state_t *state, uint64_t ticket);

/* A finished read, with the ticket of its request. error is MODULES_ERROR_NONE on success. */
unsigned modules_state_list_done(modules_state_t *state, uint64_t ticket, modules_error_t error,
                                 const modules_page_t *page);
unsigned modules_state_detail_done(modules_state_t *state, uint64_t ticket, modules_error_t error);

/* Marks a worker busy with the wanted request; NONE while one runs or nothing is wanted. */
modules_request_t modules_state_claim(modules_state_t *state);
/* The worker finished: its next request, or NONE after going idle. */
modules_request_t modules_state_finish(modules_state_t *state);
/* The claimed request could not start a worker: its view fails and the worker slot frees. */
unsigned modules_state_start_failed(modules_state_t *state);
