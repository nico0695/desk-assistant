idf_component_register(SRCS "src/board.c"
                            "src/board_power.c"
                            "src/board_audio.c"
                            "src/board_tone.c"
                            "src/audio_arbiter.c"
                            "src/board_battery.c"
                            "src/board_key.c"
                            "src/board_display.c"
                            "src/board_touch.c"
                            "src/display_geometry.c"
                            "src/display_brightness.c"
                            "src/display_sleep.c"
                            "src/touch_decode.c"
                            "src/key_filter.c"
                            "src/wake_pin.c"
                       INCLUDE_DIRS "include"
                       PRIV_INCLUDE_DIRS "src"
                       REQUIRES esp_common
                       PRIV_REQUIRES esp_driver_i2c esp_driver_i2s esp_driver_gpio esp_driver_spi esp_hw_support esp_lcd esp_pm esp_rom esp_timer freertos heap log)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
