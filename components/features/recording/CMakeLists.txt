idf_component_register(SRCS "src/recording.c"
                            "src/recording_state.c"
                            "src/recording_clip.c"
                            "src/recording_record.c"
                       INCLUDE_DIRS "include"
                       PRIV_INCLUDE_DIRS "src"
                       REQUIRES esp_common
                       PRIV_REQUIRES board bridge diagnostics esp_partition esp_timer freertos log nvs_flash storage)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
