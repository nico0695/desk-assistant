/*
 * SPDX-FileCopyrightText: 2015-2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-FileCopyrightText: 2025 Waveshare
 * SPDX-License-Identifier: Apache-2.0
 * Modified for this project; see THIRD_PARTY_NOTICES.md.
 */
#include "touch_decode.h"

#include "board_display_geometry.h"

enum {
    PACKET_STATUS_INDEX = 0,
    PACKET_X_HIGH_INDEX = 1,
    PACKET_Y_HIGH_INDEX = 2,
    PACKET_LOW_NIBBLES_INDEX = 3,
    PACKET_POINT_COUNT_INDEX = 5,
    PACKET_MARKER_INDEX = 6,
};

#define PACKET_MARKER 0xabU
#define STATUS_MASK 0x0fU
#define STATUS_PRESSED 0x06U
#define POINT_COUNT_MASK 0x7fU

static uint16_t raw_coordinate(uint8_t high, uint8_t low_nibble)
{
    return (uint16_t)(((uint16_t)high << 4) | (low_nibble & 0x0fU));
}

bool touch_decode(const uint8_t *packet, size_t length, touch_sample_t *sample)
{
    if (sample == NULL) {
        return false;
    }
    if (packet == NULL || length != TOUCH_PACKET_LENGTH || packet[PACKET_MARKER_INDEX] != PACKET_MARKER) {
        sample->pressed = false;
        return false;
    }
    if ((packet[PACKET_POINT_COUNT_INDEX] & POINT_COUNT_MASK) == 0 ||
        (packet[PACKET_STATUS_INDEX] & STATUS_MASK) != STATUS_PRESSED) {
        sample->pressed = false;
        return true;
    }

    const uint8_t low_nibbles = packet[PACKET_LOW_NIBBLES_INDEX];
    const uint16_t raw_x = raw_coordinate(packet[PACKET_X_HIGH_INDEX], low_nibbles >> 4);
    const uint16_t raw_y = raw_coordinate(packet[PACKET_Y_HIGH_INDEX], low_nibbles);
    if (raw_x >= BOARD_DISPLAY_HEIGHT_PX || raw_y >= BOARD_DISPLAY_WIDTH_PX) {
        sample->pressed = false;
        return false;
    }

    /* The panel is mirrored on raw Y and then swapped; the last valid column keeps edges inside the display. */
    sample->pressed = true;
    sample->x = (uint16_t)(BOARD_DISPLAY_WIDTH_PX - 1 - raw_y);
    sample->y = raw_x;
    return true;
}
