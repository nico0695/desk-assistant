#include "board_display_geometry.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static void assert_aligned_cover(int32_t left, int32_t top, int32_t right, int32_t bottom)
{
    int32_t x1 = left, y1 = top, x2 = right, y2 = bottom;
    board_display_align_area(&x1, &y1, &x2, &y2);

    TEST_ASSERT_TRUE(x1 >= 0 && y1 >= 0);
    TEST_ASSERT_TRUE(x2 < BOARD_DISPLAY_WIDTH_PX && y2 < BOARD_DISPLAY_HEIGHT_PX);
    TEST_ASSERT_TRUE(x1 <= left && y1 <= top && x2 >= right && y2 >= bottom);
    TEST_ASSERT_TRUE(left - x1 <= 1 && top - y1 <= 1 && x2 - right <= 1 && y2 - bottom <= 1);
    TEST_ASSERT_EQUAL_INT32(0, x1 % 2);
    TEST_ASSERT_EQUAL_INT32(0, y1 % 2);
    TEST_ASSERT_EQUAL_INT32(0, (x2 - x1 + 1) % 2);
    TEST_ASSERT_EQUAL_INT32(0, (y2 - y1 + 1) % 2);

    int32_t again_x1 = x1, again_y1 = y1, again_x2 = x2, again_y2 = y2;
    board_display_align_area(&again_x1, &again_y1, &again_x2, &again_y2);
    TEST_ASSERT_TRUE(again_x1 == x1 && again_y1 == y1 && again_x2 == x2 && again_y2 == y2);
}

static void test_full_screen_is_unchanged(void)
{
    int32_t x1 = 0, y1 = 0, x2 = BOARD_DISPLAY_WIDTH_PX - 1, y2 = BOARD_DISPLAY_HEIGHT_PX - 1;
    board_display_align_area(&x1, &y1, &x2, &y2);
    TEST_ASSERT_EQUAL_INT32(0, x1);
    TEST_ASSERT_EQUAL_INT32(0, y1);
    TEST_ASSERT_EQUAL_INT32(BOARD_DISPLAY_WIDTH_PX - 1, x2);
    TEST_ASSERT_EQUAL_INT32(BOARD_DISPLAY_HEIGHT_PX - 1, y2);
}

static void test_single_pixels_grow_to_two_by_two_inside_the_screen(void)
{
    int32_t x1 = 0, y1 = 0, x2 = 0, y2 = 0;
    board_display_align_area(&x1, &y1, &x2, &y2);
    TEST_ASSERT_TRUE(x1 == 0 && y1 == 0 && x2 == 1 && y2 == 1);

    const int32_t last = BOARD_DISPLAY_WIDTH_PX - 1;
    x1 = y1 = x2 = y2 = last;
    board_display_align_area(&x1, &y1, &x2, &y2);
    TEST_ASSERT_TRUE(x1 == last - 1 && y1 == last - 1 && x2 == last && y2 == last);
}

static void test_every_range_is_covered_and_aligned(void)
{
    for (int32_t start = 0; start < BOARD_DISPLAY_WIDTH_PX; ++start) {
        for (int32_t end = start; end < BOARD_DISPLAY_WIDTH_PX; ++end) {
            assert_aligned_cover(start, 0, end, BOARD_DISPLAY_HEIGHT_PX - 1);
            assert_aligned_cover(0, start, BOARD_DISPLAY_WIDTH_PX - 1, end);
        }
        assert_aligned_cover(start, start, start, start);
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_full_screen_is_unchanged);
    RUN_TEST(test_single_pixels_grow_to_two_by_two_inside_the_screen);
    RUN_TEST(test_every_range_is_covered_and_aligned);
    return UNITY_END();
}
