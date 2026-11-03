#include "ui.h"

#include "board_display.h"
#include "board_key.h"
#include "board_key_filter.h"
#include "board_touch.h"
#include "esp_check.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "memory_report.h"
#include "screen_power.h"
#include "sdkconfig.h"
#include "ui_display_port.h"
#include "ui_bridge_status.h"
#include "ui_navigation.h"
#include "ui_recording_control.h"
#include "ui_modules_control.h"
#include "ui_notifications.h"
#include "ui_settings_control.h"
#include "ui_status_bar.h"
#include "ui_theme.h"
#include "ui_wifi_control.h"

#define UI_TASK_NAME "ui"
#define US_PER_MS 1000
#define MS_PER_SECOND 1000U
#define HIDDEN_TOUCH_POLL_MS 33U
/* Deadlines share the screen state's sentinel so they can be compared directly. */
#define NO_DEADLINE SCREEN_POWER_NO_DEADLINE

_Static_assert(SETTINGS_SCREEN_TIMEOUT_ALWAYS_ON_S == SCREEN_POWER_ALWAYS_ON, "always-on encodings differ");

enum {
    EVENT_TOUCH = 1U << 0,
    EVENT_BATTERY = 1U << 1,
    EVENT_KEY = 1U << 2,
    EVENT_SETTINGS_CHANGED = 1U << 3,
    EVENT_CONNECTIVITY = 1U << 4,
    EVENT_BRIDGE = 1U << 5,
    EVENT_RECORDING = 1U << 6,
    EVENT_NOTIFICATIONS = 1U << 7,
    EVENT_MODULES = 1U << 8,
};

static const char *TAG = "ui";
static TaskHandle_t ui_task;
static portMUX_TYPE battery_lock = portMUX_INITIALIZER_UNLOCKED;
static ui_battery_t latest_battery;
static portMUX_TYPE bridge_lock = portMUX_INITIALIZER_UNLOCKED;
static ui_bridge_status_t latest_bridge;
static portMUX_TYPE notification_lock = portMUX_INITIALIZER_UNLOCKED;
static notification_view_t latest_notification;
static ui_notifications_handlers_t notification_handlers;
static uint64_t observed_arrival_sequence;
static bool notification_was_blocked;

static settings_loaded_t initial_settings;
static screen_power_t screen;
static board_key_filter_t key_filter;
static uint64_t key_settle_at_ms = NO_DEADLINE;

#if CONFIG_DESK_MEMORY_DIAGNOSTICS
static uint32_t wakeups_since_report;
static uint64_t next_report_at_ms;
#endif

static uint64_t uptime_ms(void)
{
    return (uint64_t)esp_timer_get_time() / US_PER_MS;
}

static uint32_t lvgl_tick_ms(void)
{
    return (uint32_t)uptime_ms();
}

static void notify_from_isr(uint32_t event)
{
    BaseType_t task_woken = pdFALSE;
    xTaskNotifyFromISR(ui_task, event, eSetBits, &task_woken);
    portYIELD_FROM_ISR(task_woken);
}

static void notify_touch_from_isr(void *unused)
{
    (void)unused;
    notify_from_isr(EVENT_TOUCH);
}

static void notify_key_from_isr(void *unused)
{
    (void)unused;
    notify_from_isr(EVENT_KEY);
}

static void show_latest_battery(void)
{
    taskENTER_CRITICAL(&battery_lock);
    const ui_battery_t battery = latest_battery;
    taskEXIT_CRITICAL(&battery_lock);
    ui_status_bar_set_battery(battery);
}

static void show_latest_bridge(void)
{
    taskENTER_CRITICAL(&bridge_lock);
    const bool online = latest_bridge.online;
    taskEXIT_CRITICAL(&bridge_lock);
    ui_status_bar_set_bridge_online(online);
}

static void apply_screen_action(screen_action_t action, const screen_power_t *previous, uint64_t now_ms)
{
    if (action == SCREEN_ACTION_NONE) {
        return;
    }
    if (action == SCREEN_ACTION_SLEEP) {
        ui_display_port_set_input_enabled(false);
        /* A gesture cut by the panel going off ends like a release. */
        ui_settings_control_end_gesture();
    }
    const esp_err_t error = board_display_set_on(action == SCREEN_ACTION_WAKE);
    if (error != ESP_OK) {
        ESP_LOGW(TAG, "panel %s failed: %s", action == SCREEN_ACTION_WAKE ? "on" : "off", esp_err_to_name(error));
        screen_power_reject_action(&screen, previous, now_ms);
    }
    ui_display_port_set_input_enabled(!screen.asleep);
}

static void apply_screen_timeout(uint8_t seconds)
{
    screen_power_set_timeout(&screen, seconds, uptime_ms());
}

static void notify_settings_changed(void)
{
    xTaskNotify(ui_task, EVENT_SETTINGS_CHANGED, eSetBits);
}

static const ui_settings_control_hooks_t SETTINGS_HOOKS = {
    .apply_timeout = apply_screen_timeout,
    .notify_changed = notify_settings_changed,
};

/* Panel off: KEY and touch wake on their next level change, also from light sleep. Panel on: back on edges. */
static void set_wake_inputs_armed(bool armed)
{
    const esp_err_t key_error = board_key_set_wake_armed(armed);
    const esp_err_t touch_error = board_touch_set_wake_armed(armed);
    if (key_error != ESP_OK || touch_error != ESP_OK) {
        ESP_LOGW(TAG, "wake inputs: key %s, touch %s", esp_err_to_name(key_error), esp_err_to_name(touch_error));
    }
}

static void update_screen(uint64_t now_ms, uint32_t screen_events)
{
    const screen_power_t previous = screen;
    const screen_action_t action = screen_power_update(&screen, now_ms, screen_events);
    apply_screen_action(action, &previous, now_ms);
}

/* Returns true once per debounced press; schedules a recheck while the level is still settling. */
static bool take_key_press(uint64_t now_ms)
{
    const bool pressed = board_key_filter_update(&key_filter, board_key_pressed(), (uint32_t)now_ms);
    const bool settling = key_filter.candidate_pressed != key_filter.stable_pressed;
    key_settle_at_ms = settling ? now_ms + BOARD_KEY_DEBOUNCE_MS : NO_DEADLINE;
    return pressed;
}

#if CONFIG_DESK_MEMORY_DIAGNOSTICS
static void report_diagnostics(uint64_t now_ms)
{
    if (now_ms < next_report_at_ms) {
        return;
    }
    lv_mem_monitor_t pool;
    lv_mem_monitor(&pool);
    ESP_LOGI(TAG, "wakeups=%u panel=%s lvgl_pool used=%u max=%u free_biggest=%u frag=%u%%",
             (unsigned)wakeups_since_report, screen.asleep ? "off" : "on",
             (unsigned)(pool.total_size - pool.free_size), (unsigned)pool.max_used, (unsigned)pool.free_biggest_size,
             (unsigned)pool.frag_pct);
    memory_report_log("ui");
    wakeups_since_report = 0;
    next_report_at_ms = now_ms + (uint64_t)CONFIG_UI_DIAGNOSTICS_PERIOD_S * MS_PER_SECOND;
}
#endif

static uint64_t next_deadline_ms(uint32_t next_lvgl_timer_ms, uint64_t now_ms)
{
    uint64_t deadline = screen_power_next_deadline_ms(&screen);
    if (next_lvgl_timer_ms != LV_NO_TIMER_READY && now_ms + next_lvgl_timer_ms < deadline) {
        deadline = now_ms + next_lvgl_timer_ms;
    }
    if (key_settle_at_ms < deadline) {
        deadline = key_settle_at_ms;
    }
    if (ui_display_port_hidden_press_held() && now_ms + HIDDEN_TOUCH_POLL_MS < deadline) {
        deadline = now_ms + HIDDEN_TOUCH_POLL_MS;
    }
#if CONFIG_DESK_MEMORY_DIAGNOSTICS
    if (next_report_at_ms < deadline) {
        deadline = next_report_at_ms;
    }
#endif
    return deadline;
}

static TickType_t ticks_until(uint64_t deadline_ms, uint64_t now_ms)
{
    if (deadline_ms == NO_DEADLINE) {
        return portMAX_DELAY;
    }
    if (deadline_ms <= now_ms) {
        return 0;
    }
    return (TickType_t)((deadline_ms - now_ms + portTICK_PERIOD_MS - 1) / portTICK_PERIOD_MS);
}

static esp_err_t start_ui(void)
{
    lv_init();
    lv_tick_set_cb(lvgl_tick_ms);
    ESP_RETURN_ON_ERROR(ui_display_port_init(notify_touch_from_isr, NULL, initial_settings.values[SETTINGS_BRIGHTNESS]), TAG, "display port");
    ESP_RETURN_ON_ERROR(board_key_init(notify_key_from_isr, NULL), TAG, "key");
    board_key_filter_init(&key_filter, board_key_pressed(), (uint32_t)uptime_ms());
    screen_power_init(&screen, initial_settings.values[SETTINGS_SCREEN_TIMEOUT], uptime_ms());
    ui_theme_init();
    ui_settings_control_init(&initial_settings, &SETTINGS_HOOKS);
    show_latest_battery();
    show_latest_bridge();
    ui_navigation_start();
    return ESP_OK;
}

static void run(void *unused)
{
    (void)unused;
    const esp_err_t error = start_ui();
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "start failed: %s", esp_err_to_name(error));
        /* Do not leave an unrendered panel lit; it fails harmlessly if the panel never started. */
        (void)board_display_set_on(false);
        /* Stay suspended so notifications from other tasks still target a valid task. */
        vTaskSuspend(NULL);
    }
    memory_report_log("ui ready");

    for (;;) {
        /* With the panel off nothing is rendered; touch and KEY still run to wake it. */
        const uint32_t next_lvgl_timer_ms = screen.asleep ? LV_NO_TIMER_READY : lv_timer_handler();
        uint64_t now_ms = uptime_ms();
        uint32_t events = 0;
        xTaskNotifyWait(0, UINT32_MAX, &events, ticks_until(next_deadline_ms(next_lvgl_timer_ms, now_ms), now_ms));
        now_ms = uptime_ms();
#if CONFIG_DESK_MEMORY_DIAGNOSTICS
        wakeups_since_report++;
#endif

        /* LVGL polls only presses it can see; a hidden press is read here until its release. */
        if ((events & EVENT_TOUCH) || ui_display_port_hidden_press_held()) {
            ui_display_port_read_touch();
        }
        if (events & EVENT_BATTERY) {
            show_latest_battery();
        }
        if (events & EVENT_BRIDGE) {
            show_latest_bridge();
        }
        if (events & EVENT_SETTINGS_CHANGED) {
            ui_settings_control_take_changes();
        }
        if (events & EVENT_CONNECTIVITY) {
            ui_wifi_control_take_view();
        }
        if (events & EVENT_RECORDING) {
            ui_recording_control_take_view();
        }
        if (events & EVENT_MODULES) {
            ui_modules_control_take_view();
        }
        bool notification_arrived = false;
        const bool notification_blocked =
            ui_recording_control_holds_screen() || ui_modules_control_holds_notifications();
        if ((events & EVENT_NOTIFICATIONS) || notification_blocked != notification_was_blocked) {
            notification_view_t view;
            ui_notifications_handlers_t handlers;
            taskENTER_CRITICAL(&notification_lock);
            view = latest_notification;
            handlers = notification_handlers;
            taskEXIT_CRITICAL(&notification_lock);
            if (view.arrival_sequence > observed_arrival_sequence) {
                observed_arrival_sequence = view.arrival_sequence;
                screen_power_notification_arrived(&screen, now_ms);
                notification_arrived = true;
            }
            if (notification_blocked) ui_notifications_hide();
            else ui_notifications_show(&view, &handlers);
            notification_was_blocked = notification_blocked;
        }
        screen_power_set_held(&screen, ui_recording_control_holds_screen(), now_ms);
        uint32_t screen_events = 0;
        if (ui_display_port_take_touch_activity()) {
            screen_events |= SCREEN_EVENT_TOUCH;
        }
        if (((events & EVENT_KEY) || now_ms >= key_settle_at_ms) && take_key_press(now_ms) &&
            !notification_arrived) {
            screen_events |= SCREEN_EVENT_KEY;
        }
        update_screen(now_ms, screen_events);
        /* Also re-arms an input that fired while the panel stayed off. */
        set_wake_inputs_armed(screen.asleep);
#if CONFIG_DESK_MEMORY_DIAGNOSTICS
        report_diagnostics(now_ms);
#endif
    }
}

esp_err_t ui_start(const settings_loaded_t *settings)
{
    ESP_RETURN_ON_FALSE(ui_task == NULL, ESP_ERR_INVALID_STATE, TAG, "already started");
    initial_settings = *settings;
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

void ui_show_bridge_online(bool online, uint64_t generation)
{
    taskENTER_CRITICAL(&bridge_lock);
    const bool changed = ui_bridge_status_update(&latest_bridge, online, generation);
    taskEXIT_CRITICAL(&bridge_lock);
    if (changed && ui_task != NULL) {
        xTaskNotify(ui_task, EVENT_BRIDGE, eSetBits);
    }
}

void ui_apply_stored_setting(settings_id_t id, uint8_t value)
{
    ui_settings_control_store_external(id, value);
    if (ui_task != NULL) {
        xTaskNotify(ui_task, EVENT_SETTINGS_CHANGED, eSetBits);
    }
}

void ui_show_connectivity(const connectivity_view_t *view)
{
    ui_wifi_control_store_view(view);
    if (ui_task != NULL) {
        xTaskNotify(ui_task, EVENT_CONNECTIVITY, eSetBits);
    }
}

void ui_show_recording(const recording_view_t *view)
{
    ui_recording_control_store_view(view);
    if (ui_task != NULL) {
        xTaskNotify(ui_task, EVENT_RECORDING, eSetBits);
    }
}

void ui_show_notification(const notification_view_t *view, void (*close)(uint64_t, const char *),
                          bool (*snooze)(uint64_t, const char *, unsigned))
{
    if (view == NULL) return;
    taskENTER_CRITICAL(&notification_lock);
    if (view->generation >= latest_notification.generation) {
        latest_notification = *view;
        notification_handlers = (ui_notifications_handlers_t){.close = close, .snooze = snooze};
    }
    taskEXIT_CRITICAL(&notification_lock);
    if (ui_task != NULL) xTaskNotify(ui_task, EVENT_NOTIFICATIONS, eSetBits);
}

void ui_show_modules(const modules_view_t *view)
{
    ui_modules_control_store_view(view);
    if (ui_task != NULL) xTaskNotify(ui_task, EVENT_MODULES, eSetBits);
}
