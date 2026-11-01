idf_component_register(SRCS "src/ui_theme.c"
                            "src/ui_widgets.c"
                            "src/ui_status_bar.c"
                            "src/ui_section.c"
                            "src/ui_home.c"
                            "src/ui_navigation.c"
                            "src/fonts/ui_font_latin1_20.c"
                            "src/fonts/ui_font_latin1_24.c"
                            "src/fonts/ui_font_latin1_28.c"
                       PRIV_INCLUDE_DIRS "src"
                       PRIV_REQUIRES board lvgl__lvgl)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
