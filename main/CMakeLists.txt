idf_component_register(SRCS "main.c"
                       PRIV_REQUIRES board diagnostics esp_system log)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
