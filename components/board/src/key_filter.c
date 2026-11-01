#include "board_key_filter.h"

void board_key_filter_init(board_key_filter_t *filter, bool pressed, uint32_t now_ms)
{
    *filter = (board_key_filter_t){
        .candidate_pressed = pressed,
        .stable_pressed = pressed,
        .candidate_since_ms = now_ms,
    };
}

bool board_key_filter_update(board_key_filter_t *filter, bool pressed, uint32_t now_ms)
{
    if (pressed != filter->candidate_pressed) {
        filter->candidate_pressed = pressed;
        filter->candidate_since_ms = now_ms;
    }
    const bool settled = (uint32_t)(now_ms - filter->candidate_since_ms) >= BOARD_KEY_DEBOUNCE_MS;
    if (filter->candidate_pressed == filter->stable_pressed || !settled) {
        return false;
    }
    filter->stable_pressed = filter->candidate_pressed;
    return filter->stable_pressed;
}
