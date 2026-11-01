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

#define MICROSECONDS_PER_MILLISECOND 1000ULL
/* Wi-Fi driver messages can include the SSID of the network. */
#define WIFI_DRIVER_LOG_TAG "wifi"

ESP_EVENT_DEFINE_BASE(STATION_EVENT);

typedef enum {
    STATION_EVENT_DEADLINE,
    STATION_EVENT_NETWORK_SAVED,
} station_event_t;

static const char *TAG = "station";
/* Owned by the default event loop task: every handler below runs there. */
static wifi_select_t selection;
static esp_timer_handle_t deadline_timer;

static uint64_t uptime_ms(void)
{
    return (uint64_t)esp_timer_get_time() / MICROSECONDS_PER_MILLISECOND;
}

static void post_event(station_event_t event)
{
    const esp_err_t error = esp_event_post(STATION_EVENT, event, NULL, 0, 0);
    if (error != ESP_OK) {
        ESP_LOGW(TAG, "event %d not posted: %s", (int)event, esp_err_to_name(error));
    }
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
        }
        break;
    case WIFI_EVENT_SCAN_DONE:
        take_scan_results();
        break;
    case WIFI_EVENT_STA_DISCONNECTED: {
        const wifi_event_sta_disconnected_t *event = data;
        const int index = disconnected_index(event);
        ESP_LOGI(TAG, "disconnected from network %d, reason %u", index, (unsigned)event->reason);
        wifi_select_disconnected(&selection, index, uptime_ms());
        break;
    }
    default:
        return;
    }
    drive();
}

static void on_got_ip(void *unused, esp_event_base_t base, int32_t id, void *data)
{
    (void)unused;
    (void)base;
    (void)id;
    (void)data;
    if (wifi_select_attempt_succeeded(&selection)) {
        ESP_LOGI(TAG, "connected to network %d", selection.target);
    }
    drive();
}

static void on_station_event(void *unused, esp_event_base_t base, int32_t id, void *data)
{
    (void)unused;
    (void)base;
    (void)data;
    if (id == STATION_EVENT_NETWORK_SAVED) {
        if (wifi_select_scan_in_progress(&selection)) {
            esp_wifi_scan_stop();
        }
        wifi_select_prefer(&selection, 0);
    }
    drive();
}

void station_network_saved(void)
{
    post_event(STATION_EVENT_NETWORK_SAVED);
}

esp_err_t station_start(void)
{
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
