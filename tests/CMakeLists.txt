cmake_minimum_required(VERSION 3.20)
project(desk_assistant_host_tests C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_EXTENSIONS ON)

if(NOT UNITY_DIR)
    message(FATAL_ERROR "UNITY_DIR is not set; run tools/test.sh")
endif()

add_library(unity STATIC ${UNITY_DIR}/unity.c)
target_include_directories(unity PUBLIC ${UNITY_DIR})

set(COMPONENTS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../components)

# Pure modules built for the host.
set(HOST_MODULE_SOURCES
    ${COMPONENTS_DIR}/board/src/display_geometry.c
    ${COMPONENTS_DIR}/board/src/display_brightness.c
    ${COMPONENTS_DIR}/board/src/touch_decode.c
    ${COMPONENTS_DIR}/board/src/key_filter.c
    ${COMPONENTS_DIR}/ui/src/screen_power.c
    ${COMPONENTS_DIR}/ui/src/touch_gate.c
    ${COMPONENTS_DIR}/features/settings/src/settings_values.c
    ${COMPONENTS_DIR}/features/settings/src/settings_row.c
    ${COMPONENTS_DIR}/connectivity/src/wifi_list.c
    ${COMPONENTS_DIR}/connectivity/src/wifi_select.c
    ${COMPONENTS_DIR}/connectivity/src/setup_credentials.c
    ${COMPONENTS_DIR}/connectivity/src/provision_line.c
    ${COMPONENTS_DIR}/connectivity/src/setup_form.c
    ${COMPONENTS_DIR}/bridge/src/bridge_origin.c)
set(HOST_MODULE_INCLUDE_DIRS
    ${COMPONENTS_DIR}/board/include
    ${COMPONENTS_DIR}/board/src
    ${COMPONENTS_DIR}/ui/src
    ${COMPONENTS_DIR}/features/settings/include
    ${COMPONENTS_DIR}/connectivity/include
    ${COMPONENTS_DIR}/connectivity/src
    ${COMPONENTS_DIR}/bridge/include)

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
