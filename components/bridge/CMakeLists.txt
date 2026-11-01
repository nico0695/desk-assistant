idf_component_register(SRCS "src/bridge_origin.c"
                            "src/bridge_origin_store.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_common
                       PRIV_REQUIRES storage)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
