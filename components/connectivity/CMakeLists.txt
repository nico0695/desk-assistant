idf_component_register(SRCS "src/connectivity.c"
                            "src/network_store.c"
                            "src/provision_usb.c"
                            "src/provision_apply.c"
                            "src/provision_line.c"
                            "src/setup_form.c"
                            "src/setup_credentials.c"
                            "src/wifi_list.c"
                            "src/wifi_select.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_common
                       PRIV_REQUIRES bridge esp_driver_usb_serial_jtag esp_timer freertos log nvs_flash storage)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
