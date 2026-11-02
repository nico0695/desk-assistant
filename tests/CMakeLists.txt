cmake_minimum_required(VERSION 3.20)
project(desk_assistant_host_tests C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_EXTENSIONS ON)

if(NOT UNITY_DIR)
    message(FATAL_ERROR "UNITY_DIR is not set; run tools/test.sh")
endif()

add_library(unity STATIC ${UNITY_DIR}/unity.c)
target_include_directories(unity PUBLIC ${UNITY_DIR})
get_filename_component(IDF_COMPONENTS_DIR "${UNITY_DIR}/../../.." ABSOLUTE)
set(CJSON_DIR "${IDF_COMPONENTS_DIR}/json/cJSON")
add_library(cjson STATIC ${CJSON_DIR}/cJSON.c)
target_include_directories(cjson PUBLIC ${CJSON_DIR})

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
    ${COMPONENTS_DIR}/connectivity/src/setup_page.c
    ${COMPONENTS_DIR}/connectivity/src/provision_apply.c
    ${COMPONENTS_DIR}/bridge/src/bridge_origin.c
    ${COMPONENTS_DIR}/bridge/src/bridge_connection.c
    ${COMPONENTS_DIR}/bridge/src/bridge_http_common.c
    ${COMPONENTS_DIR}/bridge/src/bridge_sse.c
    ${COMPONENTS_DIR}/bridge/src/bridge_uuid.c)
set(HOST_MODULE_INCLUDE_DIRS
    ${COMPONENTS_DIR}/board/include
    ${COMPONENTS_DIR}/board/src
    ${COMPONENTS_DIR}/ui/src
    ${COMPONENTS_DIR}/features/settings/include
    ${COMPONENTS_DIR}/connectivity/include
    ${COMPONENTS_DIR}/connectivity/src
    ${COMPONENTS_DIR}/bridge/include
    ${COMPONENTS_DIR}/bridge/src)

enable_testing()
file(GLOB TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/test_*.c)
foreach(test_source ${TEST_SOURCES})
    get_filename_component(test_name ${test_source} NAME_WE)
    add_executable(${test_name} ${test_source} ${HOST_MODULE_SOURCES})
    target_include_directories(${test_name} PRIVATE ${HOST_MODULE_INCLUDE_DIRS})
    target_compile_options(${test_name} PRIVATE -Wall -Wextra -Werror)
    target_link_libraries(${test_name} PRIVATE unity cjson m)
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()

add_executable(test_bridge_clock integration/test_bridge_clock.c ${COMPONENTS_DIR}/bridge/src/bridge_clock.c)
target_include_directories(test_bridge_clock PRIVATE bridge_clock_stubs ${COMPONENTS_DIR}/bridge/include)
target_compile_options(test_bridge_clock PRIVATE -Wall -Wextra -Werror)
target_link_libraries(test_bridge_clock PRIVATE unity)
add_test(NAME test_bridge_clock COMMAND test_bridge_clock)

add_executable(test_bridge_http integration/test_bridge_http.c
    ${COMPONENTS_DIR}/bridge/src/bridge_http_common.c ${COMPONENTS_DIR}/bridge/src/bridge_origin.c)
target_include_directories(test_bridge_http PRIVATE bridge_http_stubs ${COMPONENTS_DIR}/bridge/include ${COMPONENTS_DIR}/bridge/src)
target_compile_options(test_bridge_http PRIVATE -Wall -Wextra -Werror)
target_link_libraries(test_bridge_http PRIVATE unity)
add_test(NAME test_bridge_http COMMAND test_bridge_http)
