#pragma once

#include <stdbool.h>
#include <stdint.h>

#define SCREEN_POWER_DEFAULT_TIMEOUT_S 30U
#define SCREEN_POWER_ALWAYS_ON 0U
#define SCREEN_POWER_NO_DEADLINE UINT64_MAX
/* After the panel rejects a command, no new action is proposed before this delay. */
#define SCREEN_POWER_RETRY_DELAY_MS 1000U

typedef enum {
    SCREEN_ACTION_NONE,
    SCREEN_ACTION_SLEEP,
    SCREEN_ACTION_WAKE,
} screen_action_t;

enum {
    SCREEN_EVENT_TOUCH = 1U << 0,
    SCREEN_EVENT_KEY = 1U << 1,
};

typedef struct {
    bool asleep;
    bool manual_sleep;
    bool wake_pending;
    bool sleep_pending;
    uint32_t timeout_s;
    uint64_t last_activity_ms;
    uint64_t retry_at_ms;
} screen_power_t;

/* Panel awake. timeout_s is the inactivity timeout, or SCREEN_POWER_ALWAYS_ON. Times are monotonic. */
void screen_power_init(screen_power_t *state, uint32_t timeout_s, uint64_t now_ms);

/*
 * Pass TOUCH while the touch is held, not only on its first sample. KEY is one debounced press and toggles the
 * panel, also with always-on. The caller applies the returned action to the panel.
 */
screen_action_t screen_power_update(screen_power_t *state, uint64_t now_ms, uint32_t events);

/*
 * Restores the state before an action the panel could not apply, keeping a wake or manual sleep to retry
 * once SCREEN_POWER_RETRY_DELAY_MS has passed.
 */
void screen_power_reject_action(screen_power_t *state, const screen_power_t *previous, uint64_t now_ms);

/* Next time update must run without events: the timeout or the retry, or SCREEN_POWER_NO_DEADLINE. */
uint64_t screen_power_next_deadline_ms(const screen_power_t *state);
