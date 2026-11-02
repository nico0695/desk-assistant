#pragma once
#include <stddef.h>
#include "esp_err.h"
esp_err_t storage_set_blob(const char *name_space, const char *key, const void *data, size_t size);
esp_err_t storage_get_blob(const char *name_space, const char *key, void *data, size_t size);
