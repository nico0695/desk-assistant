idf_component_register(SRCS "src/recording_state.c"
                       INCLUDE_DIRS "include"
                       PRIV_INCLUDE_DIRS "src")

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
