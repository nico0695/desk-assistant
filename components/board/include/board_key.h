#pragma once

#include <stdbool.h>

#include "board.h"

/* Enables the KEY interrupt on both edges; debounce belongs to the caller. */
esp_err_t board_key_init(board_isr_cb_t on_edge, void *context);

bool board_key_pressed(void);
