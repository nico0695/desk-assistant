#include "wifi_station.h"

#include <string.h>

#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "network_store.h"
#include "wifi_select.h"
#include "wifi_setup.h"

#define MICROSECONDS_PER_MILLISECOND 1000ULL
/* Wi-Fi driver messages can include the SSID of the network. */
#define WIFI_DRIVER_LOG_TAG "wifi"

ESP_EVENT_DEFINE_BASE(STATION_EVENT);

typedef enum {
    STATION_EVENT_DEADLINE,
    STATION_EVENT_NETWORK_SAVED,
    STATION_EVENT_FORGET,
    STATION_EVENT_OPEN_SETUP,
    STATION_EVENT_CANCEL_SETUP,
} station_event_t;

static const char *TAG = "station";
/* Owned by the default event loop task: every handler below runs there. */
static wifi_select_t selection;
static esp_timer_handle_t deadline_timer;
static station_view_changed_t view_changed;
static connectivity_view_t published_view;
/* The network saved from the setup refused its password; cleared by the next save or when the setup closes. */
static bool password_rejected;

static uint64_t uptime_ms(void)
{
    return (uint64_t)esp_timer_get_time() / MICROSECONDS_PER_MILLISECOND;
}

static void post_event_with_data(station_event_t event, const void *data, size_t size)
{
    const esp_err_t error = esp_event_post(STATION_EVENT, event, data, size, 0);
    if (error != ESP_OK) {
        ESP_LOGW(TAG, "event %d not posted: %s", (int)event, esp_err_to_name(error));
    }
}

static void post_event(station_event_t event)
{
    post_event_with_data(event, NULL, 0);
}

static void on_deadline(void *unused)
{
    (void)unused;
    post_event(STATION_EVENT_DEADLINE);
}

static void start_scan(void)
{
    const esp_err_t error = esp_wifi_scan_start(NULL, false);
    if (error != ESP_OK) {
        /* A failed scan tries every saved network in saved order. */
        ESP_LOGW(TAG, "scan not started: %s", esp_err_to_name(error));
        int16_t best[WIFI_LIST_CAPACITY];
        for (unsigned i = 0; i < WIFI_LIST_CAPACITY; ++i) {
            best[i] = WIFI_SELECT_UNSEEN;
        }
        wifi_select_round(&selection, network_store_count(), best);
    }
}

static void start_attempt(uint8_t index)
{
    wifi_config_t config;
    if (!network_store_station_config(index, &config)) {
        return;
    }
    /* Ends an attempt still in progress; its late disconnect is ignored by the selection. */
    esp_wifi_disconnect();
    esp_err_t error = esp_wifi_set_config(WIFI_IF_STA, &config);
    memset(&config, 0, sizeof(config));
    if (error == ESP_OK) {
        error = esp_wifi_connect();
    }
    if (error != ESP_OK) {
        ESP_LOGW(TAG, "attempt on network %u not started: %s", (unsigned)index, esp_err_to_name(error));
    } else {
        ESP_LOGI(TAG, "trying network %u", (unsigned)index);
    }
}

/* Runs the policy until it waits, then arms the timer for its next deadline. */
static void drive(void)
{
    esp_timer_stop(deadline_timer);
    for (;;) {
        uint8_t index;
        const wifi_select_action_t action = wifi_select_step(&selection, uptime_ms(), &index);
        if (action == WIFI_SELECT_WAIT) {
            break;
        }
        if (action == WIFI_SELECT_SCAN) {
            start_scan();
        } else {
            start_attempt(index);
        }
    }
    const uint64_t wake_ms = wifi_select_wake_ms(&selection);
    if (wake_ms != WIFI_SELECT_NO_WAKE) {
        const uint64_t now_ms = uptime_ms();
        const uint64_t delay_ms = wake_ms > now_ms ? wake_ms - now_ms : 0U;
        esp_timer_start_once(deadline_timer, delay_ms * MICROSECONDS_PER_MILLISECOND);
    }
}

static connectivity_state_t current_state(uint8_t saved_count)
{
    if (saved_count == 0U) {
        return CONNECTIVITY_NO_NETWORKS;
    }
    if (selection.linked && selection.target >= 0 && selection.target < saved_count) {
        return CONNECTIVITY_CONNECTED;
    }
    return selection.attempting || selection.scanning || selection.lost ? CONNECTIVITY_SEARCHING
                                                                           : CONNECTIVITY_OFFLINE;
}

/* Tells the UI what changed since the last view; an unchanged view wakes nobody. */
static void publish_view(void)
{
    connectivity_view_t view;
    memset(&view, 0, sizeof(view));
    network_store_fill_view(&view);
    wifi_setup_fill_view(&view);
    view.setup_password_rejected = view.setup_active && password_rejected;
    view.state = current_state(view.saved_count);
    if (view.state == CONNECTIVITY_CONNECTED) {
        view.connected_index = (uint8_t)selection.target;
    }
    if (memcmp(&view, &published_view, sizeof(view)) == 0) {
        return;
    }
    published_view = view;
    view_changed(&view);
}

/* Runs the policy until it waits and tells the UI. */
static void settle(void)
{
    drive();
    publish_view();
}

static void close_setup(void)
{
    wifi_setup_close();
    password_rejected = false;
    wifi_select_set_paused(&selection, false, uptime_ms());
}

static void open_setup(void)
{
    if (wifi_setup_open(station_network_saved) == ESP_OK) {
        wifi_select_set_paused(&selection, true, uptime_ms());
    }
}

static bool reason_is_password_rejection(uint8_t reason)
{
    return reason == WIFI_REASON_AUTH_FAIL || reason == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT ||
           reason == WIFI_REASON_HANDSHAKE_TIMEOUT;
}

static void forget_network(const char *ssid)
{
    connectivity_view_t before;
    memset(&before, 0, sizeof(before));
    network_store_fill_view(&before);
    const bool linked = selection.linked && selection.target >= 0 && selection.target < before.saved_count;
    char linked_ssid[WIFI_SSID_SIZE] = {0};
    if (linked) {
        memcpy(linked_ssid, before.saved[selection.target], sizeof(linked_ssid));
    }
    const esp_err_t error = network_store_forget(ssid);
    if (error != ESP_OK) {
        ESP_LOGW(TAG, "network not forgotten: %s", esp_err_to_name(error));
        return;
    }
    if (linked && strcmp(linked_ssid, ssid) != 0) {
        /* Another network stays connected; only its position changed. */
        wifi_select_connected(&selection, (uint8_t)network_store_find(linked_ssid));
        return;
    }
    if (linked) {
        esp_wifi_disconnect();
    }
    if (network_store_count() > 0U) {
        wifi_select_reset(&selection, uptime_ms());
    } else {
        wifi_select_stop(&selection);
    }
}

static void take_scan_results(void)
{
    int16_t best[WIFI_LIST_CAPACITY];
    for (unsigned i = 0; i < WIFI_LIST_CAPACITY; ++i) {
        best[i] = WIFI_SELECT_UNSEEN;
    }
    wifi_ap_record_t record;
    unsigned seen = 0;
    while (esp_wifi_scan_get_ap_record(&record) == ESP_OK) {
        char ssid[WIFI_SSID_SIZE] = {0};
        memcpy(ssid, record.ssid, WIFI_SSID_MAX_BYTES);
        network_store_note_scan(ssid, record.rssi, best);
        ++seen;
    }
    esp_wifi_clear_ap_list();
    if (wifi_select_round(&selection, network_store_count(), best)) {
        ESP_LOGI(TAG, "scan done, %u networks heard", seen);
    }
}

static int disconnected_index(const wifi_event_sta_disconnected_t *event)
{
    char ssid[WIFI_SSID_SIZE] = {0};
    memcpy(ssid, event->ssid, event->ssid_len < WIFI_SSID_MAX_BYTES ? event->ssid_len : WIFI_SSID_MAX_BYTES);
    const int index = network_store_find(ssid);
    return index >= 0 ? index : WIFI_SELECT_UNKNOWN_NETWORK;
}

static void on_wifi_event(void *unused, esp_event_base_t base, int32_t id, void *data)
{
    (void)unused;
    (void)base;
    switch (id) {
    case WIFI_EVENT_STA_START:
        /* Without saved networks the selection stays idle until one is saved. */
        if (network_store_count() > 0) {
            wifi_select_reset(&selection, uptime_ms());
        } else if (!network_store_failed()) {
            open_setup();
        }
        break;
    case WIFI_EVENT_SCAN_DONE:
        take_scan_results();
        break;
    case WIFI_EVENT_STA_DISCONNECTED: {
        const wifi_event_sta_disconnected_t *event = data;
        const int index = disconnected_index(event);
        ESP_LOGI(TAG, "disconnected from network %d, reason %u", index, (unsigned)event->reason);
        if (wifi_setup_active() && selection.attempting && selection.preferred && index == selection.target &&
            reason_is_password_rejection(event->reason)) {
            password_rejected = true;
        }
        wifi_select_disconnected(&selection, index, uptime_ms());
        break;
    }
    default:
        return;
    }
    settle();
}

static void on_got_ip(void *unused, esp_event_base_t base, int32_t id, void *data)
{
    (void)unused;
    (void)base;
    (void)id;
    (void)data;
    if (wifi_select_attempt_succeeded(&selection)) {
        ESP_LOGI(TAG, "connected to network %d", selection.target);
        if (wifi_setup_active()) {
            close_setup();
        }
    }
    settle();
}

static void on_station_event(void *unused, esp_event_base_t base, int32_t id, void *data)
{
    (void)unused;
    (void)base;
    switch (id) {
    case STATION_EVENT_NETWORK_SAVED:
        if (wifi_select_scan_in_progress(&selection)) {
            esp_wifi_scan_stop();
        }
        password_rejected = false;
        wifi_select_prefer(&selection, 0);
        break;
    case STATION_EVENT_FORGET:
        forget_network(data);
        break;
    case STATION_EVENT_OPEN_SETUP:
        open_setup();
        break;
    case STATION_EVENT_CANCEL_SETUP:
        close_setup();
        break;
    case STATION_EVENT_DEADLINE:
    default:
        break;
    }
    settle();
}

void station_network_saved(void)
{
    post_event(STATION_EVENT_NETWORK_SAVED);
}

void station_forget(const char *ssid)
{
    char name[WIFI_SSID_SIZE] = {0};
    strncpy(name, ssid, WIFI_SSID_MAX_BYTES);
    post_event_with_data(STATION_EVENT_FORGET, name, sizeof(name));
}

void station_open_setup(void)
{
    post_event(STATION_EVENT_OPEN_SETUP);
}

void station_cancel_setup(void)
{
    post_event(STATION_EVENT_CANCEL_SETUP);
}

esp_err_t station_start(station_view_changed_t on_view_changed)
{
    view_changed = on_view_changed;
    esp_log_level_set(WIFI_DRIVER_LOG_TAG, ESP_LOG_WARN);
    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "netif");
    const esp_err_t loop_error = esp_event_loop_create_default();
    ESP_RETURN_ON_FALSE(loop_error == ESP_OK || loop_error == ESP_ERR_INVALID_STATE, loop_error, TAG, "event loop");
    ESP_RETURN_ON_FALSE(esp_netif_create_default_wifi_sta() != NULL, ESP_ERR_NO_MEM, TAG, "station netif");
    const wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&init_config), TAG, "wifi init");
    ESP_RETURN_ON_ERROR(esp_wifi_set_storage(WIFI_STORAGE_RAM), TAG, "wifi storage");
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "wifi mode");
    const esp_timer_create_args_t timer_args = {.callback = on_deadline, .name = "wifi_select"};
    ESP_RETURN_ON_ERROR(esp_timer_create(&timer_args, &deadline_timer), TAG, "timer");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_wifi_event, NULL), TAG, "wifi");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_got_ip, NULL), TAG, "ip");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(STATION_EVENT, ESP_EVENT_ANY_ID, on_station_event, NULL), TAG,
                        "station");
    return esp_wifi_start();
}
