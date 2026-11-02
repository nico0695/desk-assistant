#include "modules_state.h"

#include <string.h>

#include "bridge_snooze.h"

static const modules_request_t NO_REQUEST = {.type = MODULES_REQUEST_NONE};

void modules_state_init(modules_state_t *state)
{
    memset(state, 0, sizeof(*state));
}

void modules_state_view(const modules_state_t *state, modules_view_t *view)
{
    view->open = state->open;
    view->kind = state->kind;
    view->screen = state->screen;
    view->status = state->status;
    view->error = state->error;
    view->ticket = state->ticket;
    view->revision = 0U; /* Set by the owner when it publishes. */
    view->page = state->page;
    view->has_previous = state->has_previous;
    view->has_next = state->has_next;
    view->count = state->count;
    view->action = state->action;
    view->notice = state->notice;
    memcpy(view->snoozed_until, state->snoozed_until, sizeof(view->snoozed_until));
    const bool actionable = state->open && state->screen == MODULES_SCREEN_DETAIL &&
                            state->status == MODULES_READY && !state->fresh_required;
    view->can_delete = actionable;
    view->can_complete = actionable && state->kind == BRIDGE_RESOURCE_TASKS && state->task_pending;
    view->can_snooze = actionable && state->kind == BRIDGE_RESOURCE_ALERTS;
}

bool modules_state_is_current(const modules_state_t *state, uint64_t ticket)
{
    return state->open && state->ticket == ticket;
}

static void want(modules_state_t *state, modules_request_type_t type, uint64_t number)
{
    state->wanted = (modules_request_t){.type = type, .kind = state->kind, .number = number, .ticket = state->ticket};
    state->status = MODULES_LOADING;
    state->error = MODULES_ERROR_NONE;
}

/* A user command ends the previous notice; reads that follow an action keep it. */
static void clear_notice(modules_state_t *state)
{
    state->action = MODULES_ACTION_NONE;
    state->notice = MODULES_NOTICE_NONE;
    state->snoozed_until[0] = '\0';
}

static void forget_list(modules_state_t *state)
{
    state->has_previous = state->has_next = false;
    state->count = 0U;
    state->retained = false;
}

unsigned modules_state_enter(modules_state_t *state, bridge_resource_kind_t kind)
{
    if (kind >= BRIDGE_RESOURCE_KIND_COUNT) return MODULES_EFFECT_NONE;
    /* A worker may still be reading a request from before this entry; its identity survives the reset. */
    const bool running = state->running;
    const uint64_t ticket = state->ticket + 1U;
    const modules_request_t running_request = state->running_request;
    memset(state, 0, sizeof(*state));
    state->running = running;
    state->running_request = running_request;
    state->open = true;
    state->kind = kind;
    state->screen = MODULES_SCREEN_LIST;
    state->page = 1U;
    state->ticket = ticket;
    want(state, MODULES_REQUEST_LIST, 1U);
    return MODULES_EFFECT_VIEW | MODULES_EFFECT_WORK | MODULES_EFFECT_FREE_LIST | MODULES_EFFECT_FREE_DETAIL;
}

unsigned modules_state_leave(modules_state_t *state)
{
    if (!state->open) return MODULES_EFFECT_NONE;
    state->open = false;
    state->ticket++;
    state->wanted = NO_REQUEST;
    forget_list(state);
    clear_notice(state);
    return MODULES_EFFECT_FREE_LIST | MODULES_EFFECT_FREE_DETAIL;
}

static bool list_ready(const modules_state_t *state, uint64_t ticket)
{
    return modules_state_is_current(state, ticket) && state->screen == MODULES_SCREEN_LIST &&
           state->status == MODULES_READY;
}

static unsigned go_to_page(modules_state_t *state, uint64_t page)
{
    state->ticket++;
    state->page = page;
    forget_list(state);
    want(state, MODULES_REQUEST_LIST, page);
    return MODULES_EFFECT_VIEW | MODULES_EFFECT_WORK | MODULES_EFFECT_FREE_LIST;
}

unsigned modules_state_previous(modules_state_t *state, uint64_t ticket)
{
    if (!list_ready(state, ticket) || !state->has_previous) return MODULES_EFFECT_NONE;
    clear_notice(state);
    return go_to_page(state, state->page - 1U);
}

unsigned modules_state_next(modules_state_t *state, uint64_t ticket)
{
    if (!list_ready(state, ticket) || !state->has_next) return MODULES_EFFECT_NONE;
    clear_notice(state);
    return go_to_page(state, state->page + 1U);
}

unsigned modules_state_refresh(modules_state_t *state, uint64_t ticket)
{
    if (!modules_state_is_current(state, ticket) || state->status == MODULES_LOADING ||
        state->status == MODULES_ACTING) return MODULES_EFFECT_NONE;
    clear_notice(state);
    if (state->screen == MODULES_SCREEN_LIST) return go_to_page(state, state->page);
    state->ticket++;
    want(state, MODULES_REQUEST_DETAIL, state->detail_id);
    return MODULES_EFFECT_VIEW | MODULES_EFFECT_WORK | MODULES_EFFECT_FREE_DETAIL;
}

unsigned modules_state_open_detail(modules_state_t *state, uint64_t ticket, uint64_t id)
{
    if (!list_ready(state, ticket)) return MODULES_EFFECT_NONE;
    bool listed = false;
    for (uint8_t i = 0; i < state->count; ++i) listed = listed || state->ids[i] == id;
    if (!listed) return MODULES_EFFECT_NONE;
    clear_notice(state);
    state->fresh_required = false;
    state->task_pending = false;
    state->ticket++;
    state->screen = MODULES_SCREEN_DETAIL;
    state->retained = true;
    state->detail_id = id;
    want(state, MODULES_REQUEST_DETAIL, id);
    return MODULES_EFFECT_VIEW | MODULES_EFFECT_WORK;
}

unsigned modules_state_back(modules_state_t *state, uint64_t ticket)
{
    if (!modules_state_is_current(state, ticket) || state->screen != MODULES_SCREEN_DETAIL ||
        state->status == MODULES_ACTING) return MODULES_EFFECT_NONE;
    clear_notice(state);
    state->ticket++;
    state->wanted = NO_REQUEST;
    state->screen = MODULES_SCREEN_LIST;
    state->error = MODULES_ERROR_NONE;
    if (state->retained) {
        state->retained = false;
        state->status = MODULES_READY;
        return MODULES_EFFECT_VIEW | MODULES_EFFECT_FREE_DETAIL;
    }
    forget_list(state);
    want(state, MODULES_REQUEST_LIST, state->page);
    return MODULES_EFFECT_VIEW | MODULES_EFFECT_WORK | MODULES_EFFECT_FREE_DETAIL;
}

unsigned modules_state_list_done(modules_state_t *state, uint64_t ticket, modules_error_t error,
                                 const modules_page_t *page)
{
    if (!modules_state_is_current(state, ticket) || state->screen != MODULES_SCREEN_LIST ||
        state->status != MODULES_LOADING) return MODULES_EFFECT_DISCARD;
    if (error != MODULES_ERROR_NONE || page == NULL || page->page != state->page ||
        page->count > BRIDGE_RESOURCE_PAGE_SIZE) {
        state->status = MODULES_FAILED;
        state->error = error != MODULES_ERROR_NONE ? error : MODULES_ERROR_INVALID;
        return MODULES_EFFECT_VIEW | MODULES_EFFECT_DISCARD;
    }
    state->status = MODULES_READY;
    state->error = MODULES_ERROR_NONE;
    state->has_previous = page->has_previous;
    state->has_next = page->has_next;
    state->count = page->count;
    memcpy(state->ids, page->ids, sizeof(state->ids));
    return MODULES_EFFECT_VIEW | MODULES_EFFECT_ACCEPT;
}

unsigned modules_state_detail_done(modules_state_t *state, uint64_t ticket, modules_error_t error, bool task_pending)
{
    if (!modules_state_is_current(state, ticket) || state->screen != MODULES_SCREEN_DETAIL ||
        state->status != MODULES_LOADING) return MODULES_EFFECT_DISCARD;
    if (error != MODULES_ERROR_NONE) {
        state->status = MODULES_FAILED;
        state->error = error;
        return MODULES_EFFECT_VIEW | MODULES_EFFECT_DISCARD;
    }
    state->status = MODULES_READY;
    state->error = MODULES_ERROR_NONE;
    state->task_pending = task_pending;
    state->fresh_required = false;
    return MODULES_EFFECT_VIEW | MODULES_EFFECT_ACCEPT;
}

static bool action_allowed(const modules_state_t *state, modules_action_t action, unsigned preset,
                           const char *scheduled_at)
{
    switch (action) {
    case MODULES_ACTION_DELETE: return true;
    case MODULES_ACTION_COMPLETE: return state->kind == BRIDGE_RESOURCE_TASKS && state->task_pending;
    case MODULES_ACTION_SNOOZE:
        return state->kind == BRIDGE_RESOURCE_ALERTS && preset >= BRIDGE_SNOOZE_FIVE_MINUTES && preset <= BRIDGE_SNOOZE_TOMORROW && scheduled_at != NULL &&
               scheduled_at[0] != '\0' && strlen(scheduled_at) < BRIDGE_TIME_UTC_SIZE;
    default: return false;
    }
}

unsigned modules_state_act(modules_state_t *state, uint64_t ticket, modules_action_t action, unsigned preset,
                           const char *scheduled_at)
{
    if (!modules_state_is_current(state, ticket) || state->screen != MODULES_SCREEN_DETAIL ||
        state->status != MODULES_READY || state->fresh_required ||
        !action_allowed(state, action, preset, scheduled_at)) return MODULES_EFFECT_NONE;
    clear_notice(state);
    state->ticket++;
    state->status = MODULES_ACTING;
    state->action = action;
    state->notice = MODULES_NOTICE_WORKING;
    /* Dispatching an action ends the retained list: Back reads a fresh one. */
    state->retained = false;
    state->wanted = (modules_request_t){.type = MODULES_REQUEST_ACTION, .kind = state->kind,
                                        .number = state->detail_id, .ticket = state->ticket,
                                        .action = action, .preset = preset};
    if (action == MODULES_ACTION_SNOOZE)
        memcpy(state->wanted.scheduled_at, scheduled_at, strlen(scheduled_at) + 1U);
    return MODULES_EFFECT_VIEW | MODULES_EFFECT_WORK | MODULES_EFFECT_FREE_LIST;
}

unsigned modules_state_action_done(modules_state_t *state, uint64_t ticket, modules_notice_t outcome,
                                   const char *snoozed_until)
{
    if (!modules_state_is_current(state, ticket) || state->status != MODULES_ACTING) return MODULES_EFFECT_NONE;
    state->notice = outcome;
    state->status = MODULES_READY;
    switch (outcome) {
    case MODULES_NOTICE_DONE:
        state->ticket++;
        if (state->action == MODULES_ACTION_SNOOZE) {
            if (snoozed_until != NULL && strlen(snoozed_until) < sizeof(state->snoozed_until))
                memcpy(state->snoozed_until, snoozed_until, strlen(snoozed_until) + 1U);
            want(state, MODULES_REQUEST_DETAIL, state->detail_id);
            return MODULES_EFFECT_VIEW | MODULES_EFFECT_WORK | MODULES_EFFECT_FREE_DETAIL;
        }
        state->screen = MODULES_SCREEN_LIST;
        forget_list(state);
        want(state, MODULES_REQUEST_LIST, state->page);
        return MODULES_EFFECT_VIEW | MODULES_EFFECT_WORK | MODULES_EFFECT_FREE_DETAIL;
    case MODULES_NOTICE_NOT_FOUND:
    case MODULES_NOTICE_CHANGED:
    case MODULES_NOTICE_UNCONFIRMED:
        state->fresh_required = true;
        return MODULES_EFFECT_VIEW;
    default:
        return MODULES_EFFECT_VIEW;
    }
}

modules_request_t modules_state_claim(modules_state_t *state)
{
    if (state->running || state->wanted.type == MODULES_REQUEST_NONE) return NO_REQUEST;
    state->running = true;
    state->running_request = state->wanted;
    state->wanted = NO_REQUEST;
    return state->running_request;
}

modules_request_t modules_state_finish(modules_state_t *state)
{
    state->running = false;
    return modules_state_claim(state);
}

unsigned modules_state_start_failed(modules_state_t *state)
{
    const modules_request_t request = state->running_request;
    state->running = false;
    if (request.type == MODULES_REQUEST_LIST)
        return modules_state_list_done(state, request.ticket, MODULES_ERROR_NO_MEMORY, NULL);
    if (request.type == MODULES_REQUEST_DETAIL)
        return modules_state_detail_done(state, request.ticket, MODULES_ERROR_NO_MEMORY, false);
    if (request.type == MODULES_REQUEST_ACTION)
        return modules_state_action_done(state, request.ticket, MODULES_NOTICE_NOT_SENT, NULL);
    return MODULES_EFFECT_NONE;
}
