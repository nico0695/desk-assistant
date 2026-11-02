idf_component_register(SRCS "src/modules.c" "src/modules_state.c"
                       INCLUDE_DIRS "include" PRIV_INCLUDE_DIRS "src"
                       REQUIRES bridge esp_common
                       PRIV_REQUIRES diagnostics freertos log recording)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
