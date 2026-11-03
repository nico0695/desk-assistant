#pragma once

#include <stdbool.h>

#include "board.h"

/* Enables the KEY interrupt on both edges; debounce belongs to the caller. */
esp_err_t board_key_init(board_isr_cb_t on_edge, void *context);

bool board_key_pressed(void);

/*
 * While armed (panel off), KEY interrupts on its next level change, which also wakes the system from light sleep;
 * each fire needs a new call to arm again. Disarmed, it is back on both edges. Call from the input owner task.
 */
esp_err_t board_key_set_wake_armed(bool armed);
