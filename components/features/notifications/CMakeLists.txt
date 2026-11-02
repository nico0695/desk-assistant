idf_component_register(SRCS "src/notification_state.c" "src/notification_journal.c"
                       INCLUDE_DIRS "include" PRIV_INCLUDE_DIRS "src"
                       REQUIRES bridge esp_common PRIV_REQUIRES nvs_flash storage)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
