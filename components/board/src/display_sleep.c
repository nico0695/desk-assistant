#include "display_sleep.h"

uint32_t display_sleep_in_wait_ms(uint64_t now_ms, uint64_t sleep_out_at_ms)
{
    if (now_ms < sleep_out_at_ms) {
        return DISPLAY_SLEEP_OUT_SETTLE_MS;
    }
    const uint64_t elapsed_ms = now_ms - sleep_out_at_ms;
    return elapsed_ms >= DISPLAY_SLEEP_OUT_SETTLE_MS ? 0U : (uint32_t)(DISPLAY_SLEEP_OUT_SETTLE_MS - elapsed_ms);
}
