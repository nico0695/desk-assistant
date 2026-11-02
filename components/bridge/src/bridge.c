#include "bridge.h"

#include <string.h>

#include "bridge_clock.h"
#include "bridge_connection.h"
#include "bridge_http.h"
#include "bridge_origin_store.h"
#include "bridge_stream.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

static const char *TAG = "bridge";
static portMUX_TYPE state_lock = portMUX_INITIALIZER_UNLOCKED;
static bridge_connection_t connection;
static bridge_origin_t active_origin;
static uint64_t input_revision;
static TaskHandle_t stream_task;
static bridge_online_fn online_callback;

typedef struct {
    uint64_t ticket;
} stream_context_t;

static void wake_stream(void)
{
    if (stream_task != NULL) xTaskNotifyGive(stream_task);
}

static void clear_secret(void *buffer, size_t size)
{
    volatile unsigned char *at = buffer;
    while (size-- > 0U) *at++ = 0;
}

static void origin_changed(const bridge_origin_t *origin)
{
    taskENTER_CRITICAL(&state_lock);
    active_origin = *origin;
    bridge_connection_origin_changed(&connection, true);
    ++input_revision;
    const uint64_t generation = connection.generation;
    taskEXIT_CRITICAL(&state_lock);
    if (online_callback != NULL) online_callback(false, generation);
    wake_stream();
}

void bridge_set_ip(bool has_ip)
{
    bool changed;
    uint64_t generation = 0;
    taskENTER_CRITICAL(&state_lock);
    changed = connection.has_ip != has_ip;
    if (changed) {
        bridge_connection_set_ip(&connection, has_ip);
        ++input_revision;
        generation = connection.generation;
    }
    taskEXIT_CRITICAL(&state_lock);
    if (changed) {
        if (online_callback != NULL) online_callback(false, generation);
        wake_stream();
    }
}

static void clock_synced(void *unused)
{
    (void)unused;
    wake_stream();
}

static bool stream_current(void *context)
{
    const stream_context_t *stream = context;
    taskENTER_CRITICAL(&state_lock);
    bool current = bridge_connection_is_current(&connection, stream->ticket);
    taskEXIT_CRITICAL(&state_lock);
    return current;
}

static bool stream_opened(void *context)
{
    const stream_context_t *stream = context;
    taskENTER_CRITICAL(&state_lock);
    bool opened = bridge_connection_stream_opened(&connection, stream->ticket);
    taskEXIT_CRITICAL(&state_lock);
    if (opened) {
        online_callback(true, stream->ticket);
        ESP_LOGI(TAG, "event stream open");
    }
    return opened;
}

static void stream_worker(void *unused)
{
    (void)unused;
    bridge_sse_t stream;
    bridge_sse_init(&stream);
    uint64_t observed_revision = UINT64_MAX;
    for (;;) {
        bridge_origin_t origin;
        bool has_ip;
        bool has_origin;
        uint64_t revision;
        uint64_t generation;
        taskENTER_CRITICAL(&state_lock);
        origin = active_origin;
        has_ip = connection.has_ip;
        has_origin = connection.has_origin;
        revision = input_revision;
        generation = connection.generation;
        taskEXIT_CRITICAL(&state_lock);
        if (revision != observed_revision) {
            bridge_sse_init(&stream);
            observed_revision = revision;
            online_callback(false, generation);
        }
        if (!has_ip || !has_origin) {
            clear_secret(&origin, sizeof(origin));
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            continue;
        }
        if (!bridge_clock_ready()) {
            esp_err_t error = bridge_clock_start(clock_synced, NULL);
            clear_secret(&origin, sizeof(origin));
            ulTaskNotifyTake(pdTRUE, error == ESP_OK ? portMAX_DELAY : pdMS_TO_TICKS(BRIDGE_SSE_RETRY_FIRST_MS));
            continue;
        }
        uint64_t now = (uint64_t)esp_timer_get_time() / 1000U;
        if (!bridge_sse_retry_due(&stream, now)) {
            uint64_t due = stream.disconnected_ms + stream.retry_delay_ms;
            uint64_t wait_ms = due > now ? due - now : 0U;
            clear_secret(&origin, sizeof(origin));
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(wait_ms));
            continue;
        }
        stream_context_t context;
        taskENTER_CRITICAL(&state_lock);
        context.ticket = revision == input_revision ? bridge_connection_begin_stream(&connection) : 0;
        taskEXIT_CRITICAL(&state_lock);
        if (context.ticket != 0U) {
            bridge_stream_hooks_t hooks = {.opened = stream_opened, .current = stream_current, .context = &context};
            bridge_stream_run(&origin, &stream, &hooks);
            taskENTER_CRITICAL(&state_lock);
            bool closed = bridge_connection_stream_closed(&connection, context.ticket);
            generation = connection.generation;
            taskEXIT_CRITICAL(&state_lock);
            if (closed) online_callback(false, generation);
            if (closed) ESP_LOGI(TAG, "event stream closed");
            bridge_sse_disconnected(&stream, (uint64_t)esp_timer_get_time() / 1000U);
        }
        clear_secret(&origin, sizeof(origin));
    }
}

esp_err_t bridge_start(bridge_online_fn online_changed)
{
    if (online_changed == NULL || stream_task != NULL) return ESP_ERR_INVALID_ARG;
    esp_err_t error = bridge_http_init();
    if (error != ESP_OK) return error;
    bridge_origin_t origin;
    bool configured = bridge_origin_load(&origin) == ESP_OK;
    bridge_connection_init(&connection);
    if (configured) {
        active_origin = origin;
        bridge_connection_origin_changed(&connection, true);
    }
    clear_secret(&origin, sizeof(origin));
    online_callback = online_changed;
    if (xTaskCreate(stream_worker, "bridge_sse", CONFIG_BRIDGE_STREAM_TASK_STACK_SIZE, NULL,
                    CONFIG_BRIDGE_STREAM_TASK_PRIORITY, &stream_task) != pdPASS) {
        clear_secret(&active_origin, sizeof(active_origin));
        return ESP_ERR_NO_MEM;
    }
    bridge_origin_set_changed_callback(origin_changed);
    return ESP_OK;
}
