idf_component_register(SRCS "main.c"
                       PRIV_REQUIRES board bridge connectivity diagnostics esp_system log notifications nvs_flash recording settings storage ui)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
