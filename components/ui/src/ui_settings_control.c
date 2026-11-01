#include "ui_settings_control.h"

#include <stdbool.h>

#include "board_display.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "settings.h"
#include "settings_row.h"
#include "ui_settings.h"
#include "ui_wifi_control.h"

typedef struct {
    bool received;
    bool ok;
    uint32_t save_sequence;
} save_result_t;

static const char *TAG = "ui_settings";
static ui_settings_control_hooks_t hooks;
static settings_row_t rows[SETTINGS_COUNT];
static lv_event_cb_t back_handler;
static lv_event_cb_t wifi_handler;

typedef struct {
    bool received;
    uint8_t value;
} external_value_t;

/* Written by other tasks, read by the UI task. Only the latest result and external value of each row matter. */
static portMUX_TYPE results_lock = portMUX_INITIALIZER_UNLOCKED;
static save_result_t latest_results[SETTINGS_COUNT];
static external_value_t latest_external[SETTINGS_COUNT];

static bool apply(settings_id_t id, uint8_t value)
{
    switch (id) {
    case SETTINGS_BRIGHTNESS: {
        const esp_err_t error = board_display_set_brightness(value);
        if (error != ESP_OK) {
            ESP_LOGW(TAG, "brightness not applied: %s", esp_err_to_name(error));
        }
        return error == ESP_OK;
    }
    case SETTINGS_SCREEN_TIMEOUT:
        hooks.apply_timeout(value);
        return true;
    case SETTINGS_VOLUME:
    default:
        /* Volume takes effect with sound, in a later stage; saving it is enough here. */
        return true;
    }
}

static void store_save_result(esp_err_t result, uint32_t save_sequence, void *context)
{
    const settings_id_t id = (settings_id_t)(uintptr_t)context;
    taskENTER_CRITICAL(&results_lock);
    latest_results[id] = (save_result_t){.received = true, .ok = result == ESP_OK, .save_sequence = save_sequence};
    taskEXIT_CRITICAL(&results_lock);
    hooks.notify_changed();
}

static void run_effect(settings_row_t *row, settings_row_effect_t effect)
{
    if (effect.apply) {
        settings_row_apply_result(row, effect.value, apply(row->id, effect.value));
    }
    if (effect.save) {
        const esp_err_t error =
            settings_save(row->id, effect.value, effect.save_sequence, store_save_result, (void *)(uintptr_t)row->id);
        if (error != ESP_OK) {
            ESP_LOGW(TAG, "save not queued: %s", esp_err_to_name(error));
            settings_row_save_result(row, effect.save_sequence, false);
        }
    }
    ui_settings_show(rows);
}

static void on_drag(settings_id_t id, uint8_t value)
{
    run_effect(&rows[id], settings_row_drag(&rows[id], value));
}

static void on_release(settings_id_t id)
{
    run_effect(&rows[id], settings_row_release(&rows[id]));
}

static void on_select_timeout(uint8_t seconds)
{
    run_effect(&rows[SETTINGS_SCREEN_TIMEOUT], settings_row_select(&rows[SETTINGS_SCREEN_TIMEOUT], seconds));
}

static void on_closed(void)
{
    for (unsigned i = 0; i < SETTINGS_COUNT; ++i) {
        run_effect(&rows[i], settings_row_cancel(&rows[i]));
    }
}

static void on_back(lv_event_t *event)
{
    back_handler(event);
}

static void on_wifi(lv_event_t *event)
{
    wifi_handler(event);
}

static const ui_settings_handlers_t HANDLERS = {
    .on_drag = on_drag,
    .on_release = on_release,
    .on_select_timeout = on_select_timeout,
    .on_closed = on_closed,
    .on_back = on_back,
    .on_wifi = on_wifi,
};

void ui_settings_control_init(const settings_loaded_t *loaded, const ui_settings_control_hooks_t *control_hooks)
{
    hooks = *control_hooks;
    for (settings_id_t id = 0; id < SETTINGS_COUNT; ++id) {
        settings_row_init(&rows[id], id, loaded->values[id], loaded->load_failed[id]);
    }
}

lv_obj_t *ui_settings_control_create_screen(lv_event_cb_t on_back_clicked, lv_event_cb_t on_wifi_clicked)
{
    back_handler = on_back_clicked;
    wifi_handler = on_wifi_clicked;
    lv_obj_t *screen = ui_settings_screen_create(rows, &HANDLERS);
    ui_settings_show_wifi(ui_wifi_control_state());
    return screen;
}

void ui_settings_control_store_external(settings_id_t id, uint8_t value)
{
    if (id >= SETTINGS_COUNT) {
        return;
    }
    taskENTER_CRITICAL(&results_lock);
    latest_external[id] = (external_value_t){.received = true, .value = value};
    taskEXIT_CRITICAL(&results_lock);
}

void ui_settings_control_take_changes(void)
{
    save_result_t results[SETTINGS_COUNT];
    external_value_t externals[SETTINGS_COUNT];
    taskENTER_CRITICAL(&results_lock);
    for (unsigned i = 0; i < SETTINGS_COUNT; ++i) {
        results[i] = latest_results[i];
        latest_results[i].received = false;
        externals[i] = latest_external[i];
        latest_external[i].received = false;
    }
    taskEXIT_CRITICAL(&results_lock);

    for (unsigned i = 0; i < SETTINGS_COUNT; ++i) {
        if (results[i].received) {
            settings_row_save_result(&rows[i], results[i].save_sequence, results[i].ok);
        }
        if (externals[i].received) {
            run_effect(&rows[i], settings_row_external(&rows[i], externals[i].value));
        }
    }
    ui_settings_show(rows);
}

void ui_settings_control_end_gesture(void)
{
    for (unsigned i = 0; i < SETTINGS_COUNT; ++i) {
        run_effect(&rows[i], settings_row_release(&rows[i]));
    }
}
