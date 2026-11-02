#include "bridge_diagnostic.h"

#include "sdkconfig.h"

#if CONFIG_BRIDGE_DIAGNOSTIC_PROBE

#include <stdlib.h>
#include <string.h>

#include "bridge_http.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "memory_report.h"

static const char *TAG = "bridge_probe";
static const char EVENT_PATH[] = "/v1/device/events?limit=1";
static bool attempted;

typedef struct {
    bridge_origin_t origin;
    uint64_t ticket;
    bridge_diagnostic_current_fn current;
    char body[BRIDGE_HTTP_EVENT_BODY_MAX_BYTES];
} probe_t;

static void clear_bytes(void *buffer, size_t length)
{
    volatile unsigned char *at = buffer;
    while (length-- > 0U) *at++ = 0;
}

static void probe_worker(void *argument)
{
    probe_t *probe = argument;
    memory_report_log("bridge probe start");
    if (probe->current(probe->ticket)) {
        bridge_http_response_t result = bridge_http_get(&probe->origin, EVENT_PATH,
            BRIDGE_HTTP_EVENT_GATE_MS, BRIDGE_HTTP_EVENT_IO_MS, probe->body, sizeof(probe->body));
        if (probe->current(probe->ticket)) {
            ESP_LOGI(TAG, "read result=%d status=%d bytes=%u", (int)result.result, result.status,
                     (unsigned)result.length);
        } else {
            ESP_LOGI(TAG, "read result discarded after connection change");
        }
    } else {
        ESP_LOGI(TAG, "read skipped after connection change");
    }
    clear_bytes(probe, sizeof(*probe));
    free(probe);
    memory_report_log("bridge probe complete");
    vTaskDelete(NULL);
}

void bridge_diagnostic_start(const bridge_origin_t *origin, uint64_t ticket,
                             bridge_diagnostic_current_fn current)
{
    if (attempted || origin == NULL || current == NULL || ticket == 0U) return;
    attempted = true;
    memory_report_log("bridge SSE open");
    probe_t *probe = calloc(1, sizeof(*probe));
    if (probe == NULL) {
        ESP_LOGW(TAG, "probe allocation failed");
        return;
    }
    probe->origin = *origin;
    probe->ticket = ticket;
    probe->current = current;
    if (xTaskCreate(probe_worker, "bridge_probe", CONFIG_BRIDGE_DIAGNOSTIC_TASK_STACK_SIZE, probe,
                    CONFIG_BRIDGE_DIAGNOSTIC_TASK_PRIORITY, NULL) != pdPASS) {
        clear_bytes(probe, sizeof(*probe));
        free(probe);
        ESP_LOGW(TAG, "probe task creation failed");
    }
}

#else

void bridge_diagnostic_start(const bridge_origin_t *origin, uint64_t ticket,
                             bridge_diagnostic_current_fn current)
{
    (void)origin;
    (void)ticket;
    (void)current;
}

#endif
