#include "board_display_geometry.h"

void board_display_align_area(int32_t *x1, int32_t *y1, int32_t *x2, int32_t *y2)
{
    *x1 &= ~1;
    *y1 &= ~1;
    *x2 |= 1;
    *y2 |= 1;
}
