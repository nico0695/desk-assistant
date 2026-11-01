#include "storage.h"

#include <stdbool.h>

#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#define STORAGE_TASK_NAME "storage"

typedef struct {
    const char *name_space;
    const char *key;
    uint8_t value;
    uint32_t token;
    storage_done_cb_t on_done;
    void *context;
} write_request_t;

static const char *TAG = "storage";
static SemaphoreHandle_t nvs_lock;
static QueueHandle_t write_requests;
static bool available;

typedef esp_err_t (*nvs_write_t)(nvs_handle_t handle, const char *key, const void *data, size_t size);

static esp_err_t set_u8_entry(nvs_handle_t handle, const char *key, const void *data, size_t size)
{
    (void)size;
    return nvs_set_u8(handle, key, *(const uint8_t *)data);
}

static esp_err_t set_blob_entry(nvs_handle_t handle, const char *key, const void *data, size_t size)
{
    return nvs_set_blob(handle, key, data, size);
}

/* Opens the namespace under the NVS lock, applies one write and commits. */
static esp_err_t write_entry(const char *name_space, const char *key, nvs_write_t write, const void *data,
                             size_t size)
{
    const int64_t started_us = esp_timer_get_time();
    nvs_handle_t handle;
    xSemaphoreTake(nvs_lock, portMAX_DELAY);
    esp_err_t error = nvs_open(name_space, NVS_READWRITE, &handle);
    if (error == ESP_OK) {
        error = write(handle, key, data, size);
        if (error == ESP_OK) {
            error = nvs_commit(handle);
        }
        nvs_close(handle);
    }
    xSemaphoreGive(nvs_lock);
    if (error != ESP_OK) {
        ESP_LOGW(TAG, "write %s/%s failed: %s", name_space, key, esp_err_to_name(error));
    }
#if CONFIG_DESK_MEMORY_DIAGNOSTICS
    ESP_LOGI(TAG, "write %s/%s took %lld us", name_space, key, (long long)(esp_timer_get_time() - started_us));
#else
    (void)started_us;
#endif
    return error;
}

static void run(void *unused)
{
    (void)unused;
    write_request_t request;
    for (;;) {
        xQueueReceive(write_requests, &request, portMAX_DELAY);
        const esp_err_t error =
            write_entry(request.name_space, request.key, set_u8_entry, &request.value, sizeof(request.value));
        if (request.on_done != NULL) {
            request.on_done(error, request.token, request.context);
        }
    }
}

#if CONFIG_DESK_MEMORY_DIAGNOSTICS
static void log_usage(void)
{
    nvs_stats_t stats;
    if (nvs_get_stats(NULL, &stats) == ESP_OK) {
        ESP_LOGI(TAG, "nvs entries used=%u free=%u available=%u namespaces=%u", (unsigned)stats.used_entries,
                 (unsigned)stats.free_entries, (unsigned)stats.available_entries, (unsigned)stats.namespace_count);
    }
}
#endif

static void release_resources(void)
{
    if (write_requests != NULL) {
        vQueueDelete(write_requests);
        write_requests = NULL;
    }
    if (nvs_lock != NULL) {
        vSemaphoreDelete(nvs_lock);
        nvs_lock = NULL;
    }
}

esp_err_t storage_init(void)
{
    ESP_RETURN_ON_FALSE(!available, ESP_ERR_INVALID_STATE, TAG, "already started");
    const esp_err_t error = nvs_flash_init();
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "nvs unavailable, nothing erased: %s", esp_err_to_name(error));
        return error;
    }
    nvs_lock = xSemaphoreCreateMutex();
    write_requests = xQueueCreate(CONFIG_STORAGE_QUEUE_LENGTH, sizeof(write_request_t));
    if (nvs_lock == NULL || write_requests == NULL ||
        xTaskCreate(run, STORAGE_TASK_NAME, CONFIG_STORAGE_TASK_STACK_SIZE, NULL, CONFIG_STORAGE_TASK_PRIORITY,
                    NULL) != pdPASS) {
        release_resources();
        ESP_LOGE(TAG, "no memory for the storage task");
        return ESP_ERR_NO_MEM;
    }
    available = true;
#if CONFIG_DESK_MEMORY_DIAGNOSTICS
    log_usage();
#endif
    return ESP_OK;
}

esp_err_t storage_get_u8(const char *name_space, const char *key, uint8_t *value)
{
    if (!available) {
        return ESP_ERR_INVALID_STATE;
    }
    nvs_handle_t handle;
    xSemaphoreTake(nvs_lock, portMAX_DELAY);
    esp_err_t error = nvs_open(name_space, NVS_READONLY, &handle);
    if (error == ESP_OK) {
        error = nvs_get_u8(handle, key, value);
        nvs_close(handle);
    }
    xSemaphoreGive(nvs_lock);
    return error;
}

esp_err_t storage_set_u8_async(const char *name_space, const char *key, uint8_t value, uint32_t token,
                               storage_done_cb_t on_done, void *context)
{
    if (!available) {
        return ESP_ERR_INVALID_STATE;
    }
    const write_request_t request = {
        .name_space = name_space,
        .key = key,
        .value = value,
        .token = token,
        .on_done = on_done,
        .context = context,
    };
    return xQueueSend(write_requests, &request, 0) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

esp_err_t storage_set_u8(const char *name_space, const char *key, uint8_t value)
{
    if (!available) {
        return ESP_ERR_INVALID_STATE;
    }
    return write_entry(name_space, key, set_u8_entry, &value, sizeof(value));
}

esp_err_t storage_get_blob(const char *name_space, const char *key, void *data, size_t size)
{
    if (!available) {
        return ESP_ERR_INVALID_STATE;
    }
    nvs_handle_t handle;
    size_t stored_size = size;
    xSemaphoreTake(nvs_lock, portMAX_DELAY);
    esp_err_t error = nvs_open(name_space, NVS_READONLY, &handle);
    if (error == ESP_OK) {
        error = nvs_get_blob(handle, key, data, &stored_size);
        nvs_close(handle);
    }
    xSemaphoreGive(nvs_lock);
    if (error == ESP_OK && stored_size != size) {
        error = ESP_ERR_INVALID_SIZE;
    }
    if (error == ESP_ERR_NVS_INVALID_LENGTH) {
        error = ESP_ERR_INVALID_SIZE;
    }
    return error;
}

esp_err_t storage_set_blob(const char *name_space, const char *key, const void *data, size_t size)
{
    if (!available) {
        return ESP_ERR_INVALID_STATE;
    }
    return write_entry(name_space, key, set_blob_entry, data, size);
}
