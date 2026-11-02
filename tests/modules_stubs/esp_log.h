#pragma once
#include <stdio.h>

#include "esp_err.h"
/* Formats and discards, so log arguments are evaluated and their format strings checked on the host. */
#define ESP_LOGE(tag, ...) ((void)(tag), (void)snprintf(NULL, 0U, __VA_ARGS__))
#define ESP_LOGW(tag, ...) ((void)(tag), (void)snprintf(NULL, 0U, __VA_ARGS__))
#define ESP_LOGI(tag, ...) ((void)(tag), (void)snprintf(NULL, 0U, __VA_ARGS__))
