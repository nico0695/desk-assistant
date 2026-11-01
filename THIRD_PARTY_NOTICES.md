# Third-party notices

The project is MIT licensed (see `LICENSE`). The pieces below keep their own license and attribution.

## CST9220 touch packet decoding

`components/board/src/touch_decode.*` derives the packet layout from `esp_lcd_touch_cst9217`, modified to validate every packet and map points to the display orientation.

- Authors: Espressif Systems (Shanghai) CO LTD, Waveshare
- License: Apache-2.0 (`LICENSES/Apache-2.0.txt`)
- Source: [Waveshare-ESP32-components @ 23a6012](https://github.com/waveshareteam/Waveshare-ESP32-components/tree/23a6012d5e87d27399cf90fb851cab3689e29159/display/touch/esp_lcd_touch_cst9217)
