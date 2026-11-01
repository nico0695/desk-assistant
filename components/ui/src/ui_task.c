#include "ui.h"

#include "esp_check.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "memory_report.h"
#include "sdkconfig.h"
#include "ui_display_port.h"
#include "ui_navigation.h"
#include "ui_status_bar.h"
#include "ui_theme.h"

#define UI_TASK_NAME "ui"
#define US_PER_MS 1000

enum {
    EVENT_TOUCH = 1U << 0,
    EVENT_BATTERY = 1U << 1,
};

static const char *TAG = "ui";
static TaskHandle_t ui_task;
static portMUX_TYPE battery_lock = portMUX_INITIALIZER_UNLOCKED;
static ui_battery_t latest_battery;

static uint32_t elapsed_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / US_PER_MS);
}

static void notify_touch_from_isr(void *unused)
{
    (void)unused;
    BaseType_t task_woken = pdFALSE;
    xTaskNotifyFromISR(ui_task, EVENT_TOUCH, eSetBits, &task_woken);
    portYIELD_FROM_ISR(task_woken);
}

static TickType_t ticks_until(uint32_t next_timer_ms)
{
    if (next_timer_ms == LV_NO_TIMER_READY) {
        return portMAX_DELAY;
    }
    return (TickType_t)((next_timer_ms + portTICK_PERIOD_MS - 1) / portTICK_PERIOD_MS);
}

static void show_latest_battery(void)
{
    taskENTER_CRITICAL(&battery_lock);
    const ui_battery_t battery = latest_battery;
    taskEXIT_CRITICAL(&battery_lock);
    ui_status_bar_set_battery(battery);
}

static void run(void *unused)
{
    (void)unused;
    lv_init();
    lv_tick_set_cb(elapsed_ms);
    const esp_err_t error = ui_display_port_init(notify_touch_from_isr, NULL);
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "display port failed: %s", esp_err_to_name(error));
        /* Stay suspended so notifications from other tasks still target a valid task. */
        vTaskSuspend(NULL);
    }
    ui_theme_init();
    show_latest_battery();
    ui_navigation_start();
    memory_report_log("ui ready");

    for (;;) {
        const uint32_t next_timer_ms = lv_timer_handler();
        uint32_t events = 0;
        xTaskNotifyWait(0, UINT32_MAX, &events, ticks_until(next_timer_ms));
        if (events & EVENT_TOUCH) {
            ui_display_port_read_touch();
        }
        if (events & EVENT_BATTERY) {
            show_latest_battery();
        }
    }
}

esp_err_t ui_start(void)
{
    ESP_RETURN_ON_FALSE(ui_task == NULL, ESP_ERR_INVALID_STATE, TAG, "already started");
    const BaseType_t created =
        xTaskCreate(run, UI_TASK_NAME, CONFIG_UI_TASK_STACK_SIZE, NULL, CONFIG_UI_TASK_PRIORITY, &ui_task);
    ESP_RETURN_ON_FALSE(created == pdPASS, ESP_ERR_NO_MEM, TAG, "task");
    return ESP_OK;
}

void ui_show_battery(bool known, uint8_t percent)
{
    taskENTER_CRITICAL(&battery_lock);
    latest_battery = (ui_battery_t){.known = known, .percent = percent};
    taskEXIT_CRITICAL(&battery_lock);
    if (ui_task != NULL) {
        xTaskNotify(ui_task, EVENT_BATTERY, eSetBits);
    }
}
