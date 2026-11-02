#pragma once
#include <stdio.h>
#define ESP_LOGI(tag, ...) ((void)(tag))
#define ESP_LOGW(tag, format, ...) do { (void)(tag); if (0) printf(format, __VA_ARGS__); } while (0)
