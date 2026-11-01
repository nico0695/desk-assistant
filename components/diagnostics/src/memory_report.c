#include "memory_report.h"

#include "sdkconfig.h"

#if CONFIG_DESK_MEMORY_DIAGNOSTICS

#include <stdlib.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "memory";

/* Room for tasks created between counting and listing them. */
static const UBaseType_t TASK_LIST_SPARE_SLOTS = 4;

static void log_heap(const char *checkpoint)
{
    ESP_LOGI(TAG, "[%s] heap free=%u min=%u largest=%u", checkpoint,
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
}

static void log_task_stacks(const char *checkpoint)
{
    UBaseType_t task_capacity = uxTaskGetNumberOfTasks() + TASK_LIST_SPARE_SLOTS;
    TaskStatus_t *tasks = malloc(task_capacity * sizeof(*tasks));
    if (tasks == NULL) {
        ESP_LOGW(TAG, "[%s] no memory to list task stacks", checkpoint);
        return;
    }

    UBaseType_t task_count = uxTaskGetSystemState(tasks, task_capacity, NULL);
    if (task_count == 0) {
        ESP_LOGW(TAG, "[%s] task list changed while reading stacks", checkpoint);
    }
    for (UBaseType_t i = 0; i < task_count; i++) {
        ESP_LOGI(TAG, "[%s] stack %s free_min=%u", checkpoint, tasks[i].pcTaskName,
                 (unsigned)tasks[i].usStackHighWaterMark);
    }
    free(tasks);
}

void memory_report_log(const char *checkpoint)
{
    log_heap(checkpoint);
    log_task_stacks(checkpoint);
}

#else

void memory_report_log(const char *checkpoint)
{
    (void)checkpoint;
}

#endif
