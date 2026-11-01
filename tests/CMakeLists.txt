cmake_minimum_required(VERSION 3.20)
project(desk_assistant_host_tests C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_EXTENSIONS ON)

if(NOT UNITY_DIR)
    message(FATAL_ERROR "UNITY_DIR is not set; run tools/test.sh")
endif()

add_library(unity STATIC ${UNITY_DIR}/unity.c)
target_include_directories(unity PUBLIC ${UNITY_DIR})

# Pure modules built for the host: one source and include directory per module.
set(HOST_MODULE_SOURCES)
set(HOST_MODULE_INCLUDE_DIRS)

enable_testing()
file(GLOB TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/test_*.c)
foreach(test_source ${TEST_SOURCES})
    get_filename_component(test_name ${test_source} NAME_WE)
    add_executable(${test_name} ${test_source} ${HOST_MODULE_SOURCES})
    target_include_directories(${test_name} PRIVATE ${HOST_MODULE_INCLUDE_DIRS})
    target_compile_options(${test_name} PRIVATE -Wall -Wextra -Werror)
    target_link_libraries(${test_name} PRIVATE unity)
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()
