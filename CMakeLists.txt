cmake_minimum_required(VERSION 3.18)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
set(EXTRA_COMPONENT_DIRS components/features)
project(desk_assistant)

# IDF logs private headers at DEBUG and URL/authentication values at ERROR.
idf_component_get_property(http_client_component esp_http_client COMPONENT_LIB)
target_compile_definitions(${http_client_component} PRIVATE LOG_LOCAL_LEVEL=ESP_LOG_NONE)

# DNS failures in esp_tls.c include the private hostname even at ERROR level.
# Other TLS files retain errors; cap their DEBUG diagnostics to protect peers.
idf_component_get_property(tls_component esp-tls COMPONENT_LIB)
set_property(SOURCE "$ENV{IDF_PATH}/components/esp-tls/esp_tls.c"
             TARGET_DIRECTORY ${tls_component} APPEND PROPERTY COMPILE_DEFINITIONS LOG_LOCAL_LEVEL=ESP_LOG_NONE)
set_property(SOURCE "$ENV{IDF_PATH}/components/esp-tls/esp_tls_mbedtls.c"
             TARGET_DIRECTORY ${tls_component} APPEND PROPERTY COMPILE_DEFINITIONS LOG_LOCAL_LEVEL=ESP_LOG_INFO)
