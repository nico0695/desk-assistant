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
    ${COMPONENTS_DIR}/ui/src/ui_bridge_status.c
    ${COMPONENTS_DIR}/ui/src/touch_gate.c
    ${COMPONENTS_DIR}/features/settings/src/settings_values.c
    ${COMPONENTS_DIR}/features/settings/src/settings_row.c
    ${COMPONENTS_DIR}/features/recording/src/recording_state.c
    ${COMPONENTS_DIR}/features/notifications/src/notification_state.c
    ${COMPONENTS_DIR}/features/modules/src/modules_state.c
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
    ${COMPONENTS_DIR}/bridge/src/bridge_text.c
    ${COMPONENTS_DIR}/bridge/src/bridge_assistant.c
    ${COMPONENTS_DIR}/bridge/src/bridge_events.c
    ${COMPONENTS_DIR}/bridge/src/bridge_json.c
    ${COMPONENTS_DIR}/bridge/src/bridge_snooze.c
    ${COMPONENTS_DIR}/bridge/src/bridge_resources.c
    ${COMPONENTS_DIR}/bridge/src/bridge_time.c
    ${COMPONENTS_DIR}/bridge/src/bridge_uuid.c)
set(HOST_MODULE_INCLUDE_DIRS
    ${COMPONENTS_DIR}/board/include
    ${COMPONENTS_DIR}/board/src
    ${COMPONENTS_DIR}/ui/src
    ${COMPONENTS_DIR}/features/settings/include
    ${COMPONENTS_DIR}/features/recording/include
    ${COMPONENTS_DIR}/features/recording/src
    ${COMPONENTS_DIR}/features/notifications/include
    ${COMPONENTS_DIR}/features/modules/include
    ${COMPONENTS_DIR}/features/modules/src
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
    ${COMPONENTS_DIR}/bridge/src/bridge_http_common.c ${COMPONENTS_DIR}/bridge/src/bridge_origin.c
    ${COMPONENTS_DIR}/bridge/src/bridge_stream.c ${COMPONENTS_DIR}/bridge/src/bridge_tls.c ${COMPONENTS_DIR}/bridge/src/bridge_sse.c ${COMPONENTS_DIR}/bridge/src/bridge_text.c
    ${COMPONENTS_DIR}/bridge/src/bridge_uuid.c)
target_include_directories(test_bridge_http PRIVATE bridge_http_stubs ${COMPONENTS_DIR}/bridge/include ${COMPONENTS_DIR}/bridge/src)
target_compile_options(test_bridge_http PRIVATE -Wall -Wextra -Werror)
target_link_libraries(test_bridge_http PRIVATE unity cjson m)
add_test(NAME test_bridge_http COMMAND test_bridge_http)

add_executable(test_bridge_service integration/test_bridge_service.c
    ${COMPONENTS_DIR}/bridge/src/bridge_connection.c ${COMPONENTS_DIR}/bridge/src/bridge_sse.c ${COMPONENTS_DIR}/bridge/src/bridge_text.c
    ${COMPONENTS_DIR}/bridge/src/bridge_uuid.c ${COMPONENTS_DIR}/bridge/src/bridge_diagnostic.c
    ${COMPONENTS_DIR}/ui/src/ui_bridge_status.c)
target_include_directories(test_bridge_service PRIVATE bridge_service_stubs bridge_http_stubs ${COMPONENTS_DIR}/bridge/include ${COMPONENTS_DIR}/bridge/src ${COMPONENTS_DIR}/ui/src)
target_compile_options(test_bridge_service PRIVATE -Wall -Wextra -Werror)
target_link_libraries(test_bridge_service PRIVATE unity cjson m)
add_test(NAME test_bridge_service COMMAND test_bridge_service)

add_executable(test_bridge_origin_store integration/test_bridge_origin_store.c
    ${COMPONENTS_DIR}/bridge/src/bridge_origin_store.c ${COMPONENTS_DIR}/bridge/src/bridge_origin.c)
target_include_directories(test_bridge_origin_store PRIVATE bridge_service_stubs bridge_http_stubs ${COMPONENTS_DIR}/bridge/include)
target_compile_options(test_bridge_origin_store PRIVATE -Wall -Wextra -Werror)
target_link_libraries(test_bridge_origin_store PRIVATE unity)
add_test(NAME test_bridge_origin_store COMMAND test_bridge_origin_store)

add_executable(test_bridge_diagnostic integration/test_bridge_diagnostic.c)
target_include_directories(test_bridge_diagnostic PRIVATE bridge_diagnostic_stubs bridge_http_stubs
    ${COMPONENTS_DIR}/bridge/include ${COMPONENTS_DIR}/bridge/src)
target_compile_options(test_bridge_diagnostic PRIVATE -Wall -Wextra -Werror)
target_link_libraries(test_bridge_diagnostic PRIVATE unity)
add_test(NAME test_bridge_diagnostic COMMAND test_bridge_diagnostic)

add_executable(test_recording_clip integration/test_recording_clip.c ${COMPONENTS_DIR}/features/recording/src/recording_clip.c)
target_include_directories(test_recording_clip PRIVATE recording_clip_stubs
    ${COMPONENTS_DIR}/features/recording/include ${COMPONENTS_DIR}/features/recording/src)
target_compile_options(test_recording_clip PRIVATE -Wall -Wextra -Werror)
target_link_libraries(test_recording_clip PRIVATE unity)
add_test(NAME test_recording_clip COMMAND test_recording_clip)

add_executable(test_recording_record integration/test_recording_record.c
    ${COMPONENTS_DIR}/features/recording/src/recording_record.c ${COMPONENTS_DIR}/bridge/src/bridge_uuid.c)
target_include_directories(test_recording_record PRIVATE recording_record_stubs bridge_http_stubs
    ${COMPONENTS_DIR}/features/recording/src ${COMPONENTS_DIR}/bridge/include)
target_compile_options(test_recording_record PRIVATE -Wall -Wextra -Werror)
target_link_libraries(test_recording_record PRIVATE unity)
add_test(NAME test_recording_record COMMAND test_recording_record)

add_executable(test_notification_journal integration/test_notification_journal.c
    ${COMPONENTS_DIR}/features/notifications/src/notification_journal.c
    ${COMPONENTS_DIR}/features/notifications/src/notification_state.c
    ${COMPONENTS_DIR}/bridge/src/bridge_events.c ${COMPONENTS_DIR}/bridge/src/bridge_json.c
    ${COMPONENTS_DIR}/bridge/src/bridge_text.c ${COMPONENTS_DIR}/bridge/src/bridge_time.c ${COMPONENTS_DIR}/bridge/src/bridge_uuid.c)
target_include_directories(test_notification_journal PRIVATE recording_record_stubs bridge_http_stubs
    ${COMPONENTS_DIR}/features/notifications/include ${COMPONENTS_DIR}/bridge/include ${COMPONENTS_DIR}/bridge/src)
target_compile_options(test_notification_journal PRIVATE -Wall -Wextra -Werror)
target_link_libraries(test_notification_journal PRIVATE unity cjson m)
add_test(NAME test_notification_journal COMMAND test_notification_journal)

add_executable(test_notifications integration/test_notifications.c
    ${COMPONENTS_DIR}/features/notifications/src/notification_state.c
    ${COMPONENTS_DIR}/bridge/src/bridge_time.c
    ${COMPONENTS_DIR}/bridge/src/bridge_uuid.c)
target_include_directories(test_notifications PRIVATE notifications_stubs bridge_http_stubs
    ${COMPONENTS_DIR}/features/notifications/include ${COMPONENTS_DIR}/features/recording/include
    ${COMPONENTS_DIR}/bridge/include)
target_compile_options(test_notifications PRIVATE -Wall -Wextra -Werror)
target_link_libraries(test_notifications PRIVATE unity)
add_test(NAME test_notifications COMMAND test_notifications)

add_executable(test_audio_arbiter integration/test_audio_arbiter.c ${COMPONENTS_DIR}/board/src/audio_arbiter.c)
target_include_directories(test_audio_arbiter PRIVATE ${COMPONENTS_DIR}/board/src)
target_compile_options(test_audio_arbiter PRIVATE -Wall -Wextra -Werror)
target_link_libraries(test_audio_arbiter PRIVATE unity)
add_test(NAME test_audio_arbiter COMMAND test_audio_arbiter)

add_executable(test_notification_tones integration/test_notification_tones.c
    ${COMPONENTS_DIR}/features/notifications/src/notification_tones.c)
target_include_directories(test_notification_tones PRIVATE ${COMPONENTS_DIR}/features/notifications/src)
target_compile_options(test_notification_tones PRIVATE -Wall -Wextra -Werror)
target_link_libraries(test_notification_tones PRIVATE unity)
add_test(NAME test_notification_tones COMMAND test_notification_tones)
