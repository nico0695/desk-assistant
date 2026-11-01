idf_component_register(SRCS "src/settings_values.c"
                            "src/settings_row.c"
                       INCLUDE_DIRS "include")

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
