idf_component_register(SRCS "src/wifi_list.c"
                            "src/wifi_select.c"
                            "src/setup_credentials.c"
                            "src/provision_line.c"
                            "src/setup_form.c"
                       INCLUDE_DIRS "include")

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
