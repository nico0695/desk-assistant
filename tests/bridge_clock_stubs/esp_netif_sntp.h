#pragma once
#include <sys/time.h>
#include "esp_err.h"
typedef struct {
    const char *server;
    void (*sync_cb)(struct timeval *);
} esp_sntp_config_t;
#define ESP_NETIF_SNTP_DEFAULT_CONFIG(name) ((esp_sntp_config_t){.server = (name)})
esp_err_t esp_netif_sntp_init(const esp_sntp_config_t *config);
