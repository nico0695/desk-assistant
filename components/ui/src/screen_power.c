#include "screen_power.h"

#define MS_PER_SECOND 1000U

void screen_power_init(screen_power_t *state, uint32_t timeout_s, uint64_t now_ms)
{
    *state = (screen_power_t){.timeout_s = timeout_s, .last_activity_ms = now_ms};
}

static screen_action_t wake_panel(screen_power_t *state, uint64_t now_ms)
{
    state->asleep = false;
    state->manual_sleep = false;
    state->wake_pending = false;
    state->sleep_pending = false;
    state->last_activity_ms = now_ms;
    return SCREEN_ACTION_WAKE;
}

static screen_action_t sleep_panel(screen_power_t *state, bool manual)
{
    state->asleep = true;
    state->manual_sleep = manual;
    state->sleep_pending = false;
    return SCREEN_ACTION_SLEEP;
}

static bool timed_out(const screen_power_t *state, uint64_t now_ms)
{
    return state->timeout_s != SCREEN_POWER_ALWAYS_ON &&
           now_ms - state->last_activity_ms >= (uint64_t)state->timeout_s * MS_PER_SECOND;
}

static screen_action_t decide(screen_power_t *state, uint64_t now_ms, uint32_t events)
{
    /* A stale timestamp must not wrap the inactivity arithmetic. */
    if (now_ms < state->last_activity_ms) {
        now_ms = state->last_activity_ms;
    }
    const bool touched = (events & SCREEN_EVENT_TOUCH) != 0;
    const bool key_pressed = (events & SCREEN_EVENT_KEY) != 0;

    if (state->asleep) {
        return touched || key_pressed || state->wake_pending ? wake_panel(state, now_ms) : SCREEN_ACTION_NONE;
    }
    if (key_pressed) {
        if (state->sleep_pending) {
            /* A second press cancels a manual sleep that is still waiting for a retry. */
            state->sleep_pending = false;
            state->last_activity_ms = now_ms;
            return SCREEN_ACTION_NONE;
        }
        return sleep_panel(state, true);
    }
    if (touched) {
        state->last_activity_ms = now_ms;
        state->sleep_pending = false;
        return SCREEN_ACTION_NONE;
    }
    if (state->sleep_pending) {
        return sleep_panel(state, true);
    }
    return timed_out(state, now_ms) ? sleep_panel(state, false) : SCREEN_ACTION_NONE;
}

static void restore_with_retry(screen_power_t *state, const screen_power_t *previous)
{
    const bool attempted_wake = previous->asleep && !state->asleep;
    const bool attempted_manual_sleep = !previous->asleep && state->asleep && state->manual_sleep;
    *state = *previous;
    if (attempted_wake) {
        state->wake_pending = true;
    }
    if (attempted_manual_sleep) {
        state->sleep_pending = true;
    }
}

screen_action_t screen_power_update(screen_power_t *state, uint64_t now_ms, uint32_t events)
{
    const screen_power_t previous = *state;
    const screen_action_t action = decide(state, now_ms, events);
    if (action != SCREEN_ACTION_NONE && now_ms < previous.retry_at_ms) {
        restore_with_retry(state, &previous);
        return SCREEN_ACTION_NONE;
    }
    return action;
}

void screen_power_reject_action(screen_power_t *state, const screen_power_t *previous, uint64_t now_ms)
{
    restore_with_retry(state, previous);
    state->retry_at_ms = now_ms + SCREEN_POWER_RETRY_DELAY_MS;
}

uint64_t screen_power_next_deadline_ms(const screen_power_t *state)
{
    uint64_t deadline = SCREEN_POWER_NO_DEADLINE;
    if (state->wake_pending || state->sleep_pending) {
        deadline = state->last_activity_ms;
    } else if (!state->asleep && state->timeout_s != SCREEN_POWER_ALWAYS_ON) {
        deadline = state->last_activity_ms + (uint64_t)state->timeout_s * MS_PER_SECOND;
    }
    if (deadline != SCREEN_POWER_NO_DEADLINE && deadline < state->retry_at_ms) {
        deadline = state->retry_at_ms;
    }
    return deadline;
}
