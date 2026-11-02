idf_component_register(SRCS "src/notifications.c" "src/notification_state.c" "src/notification_journal.c"
                            "src/notification_sound.c" "src/notification_tones.c"
                       INCLUDE_DIRS "include" PRIV_INCLUDE_DIRS "src"
                       REQUIRES bridge esp_common recording PRIV_REQUIRES board settings esp_timer freertos log nvs_flash storage)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
