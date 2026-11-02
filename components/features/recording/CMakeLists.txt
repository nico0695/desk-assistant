idf_component_register(SRCS "src/recording_state.c"
                            "src/recording_clip.c"
                       INCLUDE_DIRS "include"
                       PRIV_INCLUDE_DIRS "src"
                       REQUIRES esp_common
                       PRIV_REQUIRES esp_partition freertos)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
