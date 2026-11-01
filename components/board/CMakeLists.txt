idf_component_register(SRCS "src/board.c"
                            "src/board_power.c"
                            "src/board_battery.c"
                            "src/board_key.c"
                            "src/board_display.c"
                            "src/board_touch.c"
                            "src/display_geometry.c"
                            "src/display_brightness.c"
                            "src/touch_decode.c"
                            "src/key_filter.c"
                       INCLUDE_DIRS "include"
                       PRIV_INCLUDE_DIRS "src"
                       REQUIRES esp_common
                       PRIV_REQUIRES esp_driver_i2c esp_driver_gpio esp_driver_spi esp_lcd freertos heap log)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
