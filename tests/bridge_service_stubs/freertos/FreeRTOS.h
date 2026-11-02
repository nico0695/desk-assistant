#pragma once
#include <stdint.h>
typedef unsigned int TickType_t;
typedef int BaseType_t;
typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
extern unsigned bridge_test_critical_depth;
#define taskENTER_CRITICAL(lock) ((void)(lock), ++bridge_test_critical_depth)
#define taskEXIT_CRITICAL(lock) ((void)(lock), --bridge_test_critical_depth)
#define pdTRUE 1
#define pdPASS 1
#define portMAX_DELAY UINT32_MAX
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
