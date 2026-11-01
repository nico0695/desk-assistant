#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "board.h"
#include "board_display_geometry.h"

typedef struct {
    uint8_t *first;
    uint8_t *second;
    size_t size_bytes;
} board_display_buffers_t;

/*
 * Starts the panel and allocates the draw buffers in DMA-capable internal RAM.
 * on_transfer_done runs in ISR context when a draw transfer finishes. second is NULL with one buffer.
 */
esp_err_t board_display_init(board_isr_cb_t on_transfer_done, void *context, board_display_buffers_t *buffers);

/*
 * Starts an asynchronous transfer of an area aligned with board_display_align_area.
 * Pixels are RGB565 and are byte-swapped in place for the panel; the buffer stays busy until on_transfer_done.
 * On error no transfer is started and on_transfer_done is not called.
 */
esp_err_t board_display_draw(int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint8_t *pixels);

/* Display OFF/ON only; the panel keeps its configuration. */
esp_err_t board_display_set_on(bool on);

esp_err_t board_display_set_brightness(unsigned percent);
