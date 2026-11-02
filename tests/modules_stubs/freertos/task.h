#pragma once
#include "freertos/FreeRTOS.h"
typedef void *TaskHandle_t;
BaseType_t xTaskCreate(void (*task)(void *), const char *name, unsigned stack, void *arg, unsigned priority,
                       TaskHandle_t *handle);
void vTaskDelete(TaskHandle_t task);
