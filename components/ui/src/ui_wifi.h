#pragma once

#include "connectivity_view.h"
#include "lvgl.h"

/* Taps only dispatch to these handlers, which never wait; the screen shows what ui_wifi_show last received. */
typedef struct {
    /* The SSID is valid only during the call. */
    void (*on_forget)(const char *ssid);
    void (*on_add)(void);
    void (*on_cancel_setup)(void);
    lv_event_cb_t on_back;
} ui_wifi_handlers_t;

typedef enum {
    UI_WIFI_DIALOG_FORGET,
    UI_WIFI_DIALOG_ADD,
} ui_wifi_dialog_t;

/* Wi-Fi screen for the view (copied); handlers must outlive the screen. */
lv_obj_t *ui_wifi_screen_create(const connectivity_view_t *view, const ui_wifi_handlers_t *handlers);

/*
 * Refreshes the open Wi-Fi screen, if any, keeping the list scroll. An open dialog stays open until what it asks about
 * no longer holds: the forgotten network is gone, the Add dialog needs a connected network, and setup closes both.
 */
void ui_wifi_show(const connectivity_view_t *view);

/* Opens a confirmation over the open screen; the SSID is copied and used only by the forget dialog. */
void ui_wifi_open_dialog(ui_wifi_dialog_t kind, const char *ssid);
