#pragma once

#include <stdint.h>

#define BOARD_DISPLAY_WIDTH_PX 480
#define BOARD_DISPLAY_HEIGHT_PX 480

/* The CO5300 accepts partial updates only on a 2x2 grid. Apply before rendering, never in flush. */
void board_display_align_area(int32_t *x1, int32_t *y1, int32_t *x2, int32_t *y2);
