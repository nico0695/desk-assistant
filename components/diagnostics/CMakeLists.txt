idf_component_register(SRCS "src/memory_report.c"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES heap freertos log)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
