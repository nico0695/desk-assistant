#pragma once

#include <stdint.h>

/* CO5300: the booster needs this long after a sleep-out, and a sleep-in may not come sooner than that. */
#define DISPLAY_SLEEP_OUT_SETTLE_MS 120U

/* Time a sleep-in still has to wait so it comes at least DISPLAY_SLEEP_OUT_SETTLE_MS after the last sleep-out. */
uint32_t display_sleep_in_wait_ms(uint64_t now_ms, uint64_t sleep_out_at_ms);
