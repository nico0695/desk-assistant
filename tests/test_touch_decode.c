#include "touch_decode.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

/* Wire layout: status, X high, Y high, X/Y low nibbles, strength, point count, marker, padding. */
static const uint8_t PRESS_PACKET[TOUCH_PACKET_LENGTH] = {0x06, 0x06, 0x0c, 0x48, 1, 1, 0xab, 0, 0, 0};
static const uint16_t PRESS_X = 279;
static const uint16_t PRESS_Y = 100;

static void assert_rejected_and_released(const uint8_t *packet, size_t length)
{
    board_touch_sample_t sample = {.pressed = true, .x = 17, .y = 23};
    TEST_ASSERT_FALSE(touch_decode(packet, length, &sample));
    TEST_ASSERT_FALSE(sample.pressed);
    TEST_ASSERT_EQUAL_UINT16(17, sample.x);
    TEST_ASSERT_EQUAL_UINT16(23, sample.y);
}

static void test_press_maps_to_display_orientation(void)
{
    board_touch_sample_t sample = {0};
    TEST_ASSERT_TRUE(touch_decode(PRESS_PACKET, sizeof(PRESS_PACKET), &sample));
    TEST_ASSERT_TRUE(sample.pressed);
    TEST_ASSERT_EQUAL_UINT16(PRESS_X, sample.x);
    TEST_ASSERT_EQUAL_UINT16(PRESS_Y, sample.y);
}

static void test_raw_corners_stay_inside_the_display(void)
{
    static const struct {
        uint8_t packet[TOUCH_PACKET_LENGTH];
        uint16_t x;
        uint16_t y;
    } corners[] = {
        {{0x06, 0x00, 0x00, 0x00, 1, 1, 0xab, 0, 0, 0}, 479, 0},
        {{0x06, 0x1d, 0x00, 0xf0, 1, 1, 0xab, 0, 0, 0}, 479, 479},
        {{0x06, 0x00, 0x1d, 0x0f, 1, 1, 0xab, 0, 0, 0}, 0, 0},
        {{0x06, 0x1d, 0x1d, 0xff, 1, 1, 0xab, 0, 0, 0}, 0, 479},
    };
    for (size_t i = 0; i < sizeof(corners) / sizeof(corners[0]); ++i) {
        board_touch_sample_t sample = {0};
        TEST_ASSERT_TRUE(touch_decode(corners[i].packet, TOUCH_PACKET_LENGTH, &sample));
        TEST_ASSERT_TRUE(sample.pressed);
        TEST_ASSERT_EQUAL_UINT16(corners[i].x, sample.x);
        TEST_ASSERT_EQUAL_UINT16(corners[i].y, sample.y);
    }
}

static void test_malformed_packets_release_without_moving(void)
{
    const uint8_t bad_marker[TOUCH_PACKET_LENGTH] = {0x06, 1, 1, 0, 1, 1, 0, 0, 0, 0};
    assert_rejected_and_released(bad_marker, sizeof(bad_marker));
    for (size_t length = 0; length < TOUCH_PACKET_LENGTH; ++length) {
        assert_rejected_and_released(PRESS_PACKET, length);
    }
    assert_rejected_and_released(PRESS_PACKET, TOUCH_PACKET_LENGTH + 1);
    assert_rejected_and_released(NULL, TOUCH_PACKET_LENGTH);
}

static void test_out_of_range_points_are_rejected(void)
{
    const uint8_t bad_x[TOUCH_PACKET_LENGTH] = {0x06, 0x1e, 0, 0, 1, 1, 0xab, 0, 0, 0};
    const uint8_t bad_y[TOUCH_PACKET_LENGTH] = {0x06, 0, 0x1e, 0, 1, 1, 0xab, 0, 0, 0};
    assert_rejected_and_released(bad_x, sizeof(bad_x));
    assert_rejected_and_released(bad_y, sizeof(bad_y));
}

static void test_missing_sample_is_rejected(void)
{
    TEST_ASSERT_FALSE(touch_decode(PRESS_PACKET, sizeof(PRESS_PACKET), NULL));
}

static void test_release_keeps_last_coordinates(void)
{
    board_touch_sample_t sample = {0};
    TEST_ASSERT_TRUE(touch_decode(PRESS_PACKET, sizeof(PRESS_PACKET), &sample));

    const uint8_t released[TOUCH_PACKET_LENGTH] = {0x00, 0xff, 0xff, 0xff, 0, 1, 0xab, 0, 0, 0};
    TEST_ASSERT_TRUE(touch_decode(released, sizeof(released), &sample));
    TEST_ASSERT_FALSE(sample.pressed);
    TEST_ASSERT_EQUAL_UINT16(PRESS_X, sample.x);
    TEST_ASSERT_EQUAL_UINT16(PRESS_Y, sample.y);

    const uint8_t no_points[TOUCH_PACKET_LENGTH] = {0xff, 0xff, 0xff, 0xff, 0, 0, 0xab, 0, 0, 0};
    TEST_ASSERT_TRUE(touch_decode(no_points, sizeof(no_points), &sample));
    TEST_ASSERT_FALSE(sample.pressed);
    TEST_ASSERT_EQUAL_UINT16(PRESS_X, sample.x);
    TEST_ASSERT_EQUAL_UINT16(PRESS_Y, sample.y);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_press_maps_to_display_orientation);
    RUN_TEST(test_raw_corners_stay_inside_the_display);
    RUN_TEST(test_malformed_packets_release_without_moving);
    RUN_TEST(test_out_of_range_points_are_rejected);
    RUN_TEST(test_missing_sample_is_rejected);
    RUN_TEST(test_release_keeps_last_coordinates);
    return UNITY_END();
}
