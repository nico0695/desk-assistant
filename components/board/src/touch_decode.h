/*
 * CST9220 packet layout derived from Waveshare's esp_lcd_touch_cst9217 driver.
 * SPDX-FileCopyrightText: 2015-2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-FileCopyrightText: 2025 Waveshare
 * SPDX-License-Identifier: Apache-2.0
 * Modified for this project; see THIRD_PARTY_NOTICES.md.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TOUCH_PACKET_LENGTH 10U

typedef struct {
    bool pressed;
    uint16_t x;
    uint16_t y;
} touch_sample_t;

/*
 * Returns false for a malformed packet or out-of-range point; the sample is then released, never pressed.
 * A valid release keeps the last coordinates. Coordinates are mapped to the display orientation.
 */
bool touch_decode(const uint8_t *packet, size_t length, touch_sample_t *sample);
