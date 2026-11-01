#include "wifi_select.h"

#include <string.h>

static void clear_keeping_pause(wifi_select_t *state)
{
    const bool paused = state->paused;
    memset(state, 0, sizeof(*state));
    state->target = -1;
    state->paused = paused;
}

static void request_scan(wifi_select_t *state, uint64_t at_ms)
{
    state->count = state->next = 0;
    state->full_round = false;
    state->preferred = false;
    state->scan_pending = true;
    state->scan_ms = at_ms;
}

static void start_attempt(wifi_select_t *state, uint8_t index, uint64_t now_ms)
{
    state->target = (int8_t)index;
    state->attempting = true;
    state->attempt_end_ms = now_ms + WIFI_SELECT_ATTEMPT_MS;
}

/* A round entry is left and may run now: setup only lets the preferred network through. */
static bool candidate_ready(const wifi_select_t *state)
{
    return state->next < state->count && (!state->paused || state->preferred);
}

static bool scan_allowed(const wifi_select_t *state)
{
    return state->scan_pending && !state->paused;
}

void wifi_select_reset(wifi_select_t *state, uint64_t now_ms)
{
    clear_keeping_pause(state);
    request_scan(state, now_ms);
}

void wifi_select_prefer(wifi_select_t *state, uint8_t index)
{
    clear_keeping_pause(state);
    state->order[0] = index;
    state->count = 1;
    state->preferred = true;
}

void wifi_select_set_paused(wifi_select_t *state, bool paused, uint64_t now_ms)
{
    state->paused = paused;
    if (paused) {
        /* A round in progress stops; it starts over with a scan when setup closes. */
        if (!state->preferred && state->next < state->count) {
            request_scan(state, now_ms);
        }
    } else if (state->scan_pending) {
        state->scan_ms = now_ms;
    }
}

void wifi_select_note(const wifi_list_t *list, const char *ssid, int rssi, int16_t best[WIFI_LIST_CAPACITY])
{
    const int index = wifi_list_find(list, ssid);
    if (index >= 0 && rssi > best[index]) {
        best[index] = (int16_t)rssi;
    }
}

bool wifi_select_round(wifi_select_t *state, uint8_t saved, const int16_t best[WIFI_LIST_CAPACITY])
{
    if (!state->scanning) {
        return false;
    }
    if (saved > WIFI_LIST_CAPACITY) {
        saved = WIFI_LIST_CAPACITY;
    }
    state->scanning = false;
    state->count = state->next = 0;
    state->full_round = true;
    state->preferred = false;
    for (uint8_t i = 0; i < saved; ++i) {
        if (best[i] == WIFI_SELECT_UNSEEN) {
            continue;
        }
        uint8_t at = state->count++;
        while (at > 0 && best[state->order[at - 1]] < best[i]) {
            state->order[at] = state->order[at - 1];
            --at;
        }
        state->order[at] = i;
    }
    for (uint8_t i = 0; i < saved; ++i) {
        if (best[i] == WIFI_SELECT_UNSEEN) {
            state->order[state->count++] = i;
        }
    }
    return true;
}

void wifi_select_connected(wifi_select_t *state, uint8_t index)
{
    clear_keeping_pause(state);
    state->target = (int8_t)index;
    state->linked = true;
}

void wifi_select_disconnected(wifi_select_t *state, int index, uint64_t now_ms)
{
    if (state->linked) {
        if (index != state->target && index != WIFI_SELECT_UNKNOWN_NETWORK) {
            return;
        }
        state->linked = false;
        state->lost = true;
        state->lost_end_ms = now_ms + WIFI_SELECT_LOSS_WINDOW_MS;
        state->retry_ms = now_ms;
    } else if (state->attempting && index == state->target) {
        state->attempting = false;
    }
}

wifi_select_action_t wifi_select_step(wifi_select_t *state, uint64_t now_ms, uint8_t *index)
{
    if (state->linked || state->scanning) {
        return WIFI_SELECT_WAIT;
    }
    if (state->attempting) {
        if (now_ms < state->attempt_end_ms) {
            return WIFI_SELECT_WAIT;
        }
        state->attempting = false;
    }
    if (state->lost) {
        if (now_ms < state->lost_end_ms) {
            if (now_ms < state->retry_ms) {
                return WIFI_SELECT_WAIT;
            }
            state->retry_ms = now_ms + WIFI_SELECT_LOSS_RETRY_MS;
            *index = (uint8_t)state->target;
            start_attempt(state, *index, now_ms);
            return WIFI_SELECT_CONNECT;
        }
        state->lost = false;
        request_scan(state, now_ms);
    }
    if (candidate_ready(state)) {
        *index = state->order[state->next++];
        start_attempt(state, *index, now_ms);
        return WIFI_SELECT_CONNECT;
    }
    if (state->count > 0) {
        request_scan(state, now_ms + (state->full_round ? WIFI_SELECT_ROUND_WAIT_MS : 0U));
    }
    if (scan_allowed(state) && now_ms >= state->scan_ms) {
        state->scan_pending = false;
        state->scanning = true;
        return WIFI_SELECT_SCAN;
    }
    return WIFI_SELECT_WAIT;
}

uint64_t wifi_select_wake_ms(const wifi_select_t *state)
{
    if (state->linked || state->scanning) {
        return WIFI_SELECT_NO_WAKE;
    }
    if (state->attempting) {
        return state->attempt_end_ms;
    }
    if (state->lost) {
        return state->retry_ms < state->lost_end_ms ? state->retry_ms : state->lost_end_ms;
    }
    return scan_allowed(state) ? state->scan_ms : WIFI_SELECT_NO_WAKE;
}
