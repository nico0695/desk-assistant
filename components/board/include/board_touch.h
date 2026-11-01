#pragma once

#include "board.h"
#include "board_touch_sample.h"

/* Resets the CST9220 and enables its interrupt, which calls on_interrupt when new data is ready. */
esp_err_t board_touch_init(board_isr_cb_t on_interrupt, void *context);

/*
 * Reads one sample. On a bus error or an invalid packet it returns an error and the sample is released,
 * keeping the last coordinates. Blocks for a few milliseconds; call from the input owner task.
 */
esp_err_t board_touch_read(board_touch_sample_t *sample);
