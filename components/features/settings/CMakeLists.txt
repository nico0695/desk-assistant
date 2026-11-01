idf_component_register(SRCS "src/settings.c"
                            "src/settings_values.c"
                            "src/settings_row.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_common
                       PRIV_REQUIRES log nvs_flash storage)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
