idf_component_register(SRCS "src/display_geometry.c"
                            "src/display_brightness.c"
                            "src/touch_decode.c"
                            "src/key_filter.c"
                       INCLUDE_DIRS "include"
                       PRIV_INCLUDE_DIRS "src")

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
