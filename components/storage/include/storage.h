#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

/* Called from the storage task with the write result and the caller's token and context. */
typedef void (*storage_done_cb_t)(esp_err_t result, uint32_t token, void *context);

/*
 * Mounts NVS and starts the storage task. NVS is never erased: on corruption, missing free pages or a newer
 * format the error is returned and storage stays unavailable.
 */
esp_err_t storage_init(void);

/* Synchronous read. ESP_ERR_NVS_NOT_FOUND when the key or its namespace is missing. */
esp_err_t storage_get_u8(const char *name_space, const char *key, uint8_t *value);

/*
 * Queues a write and commit and returns at once; on_done runs from the storage task. name_space and key must
 * outlive the request (string literals). Fails without queuing: ESP_ERR_INVALID_STATE when storage is unavailable,
 * ESP_ERR_NO_MEM when the queue is full.
 */
esp_err_t storage_set_u8_async(const char *name_space, const char *key, uint8_t value, uint32_t token,
                               storage_done_cb_t on_done, void *context);

/*
 * Synchronous writes and commit, for callers outside the UI task; they wait for the NVS lock and the flash write.
 * ESP_ERR_INVALID_STATE when storage is unavailable.
 */
esp_err_t storage_set_u8(const char *name_space, const char *key, uint8_t value);
esp_err_t storage_set_blob(const char *name_space, const char *key, const void *data, size_t size);

/*
 * Synchronous read of a fixed-size record. ESP_ERR_NVS_NOT_FOUND when the key or its namespace is missing;
 * ESP_ERR_INVALID_SIZE when the stored size differs (an incompatible record). The buffer is undefined on error.
 */
esp_err_t storage_get_blob(const char *name_space, const char *key, void *data, size_t size);
