#pragma once

#include <stdbool.h>
#include <stdint.h>

#define BOARD_KEY_DEBOUNCE_MS 30U

typedef struct {
    bool candidate_pressed;
    bool stable_pressed;
    uint32_t candidate_since_ms;
} board_key_filter_t;

/* A key held at start is the stable state, not a new press. */
void board_key_filter_init(board_key_filter_t *filter, bool pressed, uint32_t now_ms);

/* Returns true once per press, when it has been stable for the debounce time. Timestamps may wrap. */
bool board_key_filter_update(board_key_filter_t *filter, bool pressed, uint32_t now_ms);
