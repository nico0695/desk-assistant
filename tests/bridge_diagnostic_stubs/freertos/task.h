#pragma once
#include "freertos/FreeRTOS.h"
BaseType_t xTaskCreate(void (*task)(void *), const char *name, unsigned stack, void *argument,
                       unsigned priority, TaskHandle_t *handle);
void vTaskDelete(TaskHandle_t task);
