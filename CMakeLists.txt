cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
set(EXTRA_COMPONENT_DIRS components/features)
project(desk_assistant)

# IDF's HTTP client logs complete headers at DEBUG, including Authorization.
# Cap only that component at INFO even when global diagnostic logging is higher.
idf_component_get_property(http_client_component esp_http_client COMPONENT_LIB)
target_compile_definitions(${http_client_component} PRIVATE LOG_LOCAL_LEVEL=ESP_LOG_INFO)
