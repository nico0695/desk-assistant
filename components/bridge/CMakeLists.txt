idf_component_register(SRCS "src/bridge.c"
                            "src/bridge_diagnostic.c"
                            "src/bridge_stream.c"
                            "src/bridge_origin.c"
                            "src/bridge_connection.c"
                            "src/bridge_clock.c"
                            "src/bridge_http.c"
                            "src/bridge_http_common.c"
                            "src/bridge_tls.c"
                            "src/bridge_origin_store.c"
                            "src/bridge_sse.c"
                            "src/bridge_text.c"
                            "src/bridge_assistant.c"
                            "src/bridge_uuid.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_common
                       PRIV_REQUIRES diagnostics esp_http_client esp_netif esp_timer esp-tls freertos json log lwip mbedtls storage tcp_transport)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
