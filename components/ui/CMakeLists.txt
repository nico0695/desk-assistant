idf_component_register(SRCS "src/ui_task.c"
                            "src/ui_display_port.c"
                            "src/ui_theme.c"
                            "src/ui_widgets.c"
                            "src/ui_status_bar.c"
                            "src/ui_section.c"
                            "src/ui_home.c"
                            "src/ui_navigation.c"
                            "src/ui_settings.c"
                            "src/screen_power.c"
                            "src/touch_gate.c"
                            "src/fonts/ui_font_latin1_20.c"
                            "src/fonts/ui_font_latin1_24.c"
                            "src/fonts/ui_font_latin1_28.c"
                       INCLUDE_DIRS "include"
                       PRIV_INCLUDE_DIRS "src"
                       REQUIRES esp_common settings
                       PRIV_REQUIRES board diagnostics esp_timer freertos log lvgl__lvgl)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)

