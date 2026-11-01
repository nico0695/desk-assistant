#include "ui_wifi_control.h"

#include "connectivity.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ui_settings.h"
#include "ui_wifi.h"

static portMUX_TYPE view_lock = portMUX_INITIALIZER_UNLOCKED;
/* Written by any task under view_lock; applied is the UI task's copy. */
static connectivity_view_t latest_view;
static connectivity_view_t applied_view;
static lv_event_cb_t back_handler;

static void forget(const char *ssid)
{
    connectivity_forget(ssid);
}

static void add_network(void)
{
    connectivity_open_setup();
}

static void cancel_setup(void)
{
    connectivity_cancel_setup();
}

static void closed(void) {}

static void on_back(lv_event_t *event)
{
    back_handler(event);
}

static const ui_wifi_handlers_t HANDLERS = {
    .on_forget = forget,
    .on_add = add_network,
    .on_cancel_setup = cancel_setup,
    .on_closed = closed,
    .on_back = on_back,
};

void ui_wifi_control_store_view(const connectivity_view_t *view)
{
    taskENTER_CRITICAL(&view_lock);
    latest_view = *view;
    taskEXIT_CRITICAL(&view_lock);
}

void ui_wifi_control_take_view(void)
{
    taskENTER_CRITICAL(&view_lock);
    applied_view = latest_view;
    taskEXIT_CRITICAL(&view_lock);
    ui_wifi_show(&applied_view);
    ui_settings_show_wifi(applied_view.state);
}

connectivity_state_t ui_wifi_control_state(void)
{
    return applied_view.state;
}

lv_obj_t *ui_wifi_control_create_screen(lv_event_cb_t on_back_clicked)
{
    back_handler = on_back_clicked;
    return ui_wifi_screen_create(&applied_view, &HANDLERS);
}
