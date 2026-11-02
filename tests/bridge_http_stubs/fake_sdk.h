#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
typedef uint32_t TickType_t;
#define portTICK_PERIOD_MS 1
#define pdTRUE 1
#define pdMS_TO_TICKS(ms) (ms)
typedef void *SemaphoreHandle_t;
SemaphoreHandle_t xSemaphoreCreateMutex(void);
int xSemaphoreTake(SemaphoreHandle_t gate, TickType_t wait);
int xSemaphoreGive(SemaphoreHandle_t gate);
void vSemaphoreDelete(SemaphoreHandle_t gate);
void vTaskDelay(TickType_t ticks);
int64_t esp_timer_get_time(void);
esp_err_t esp_crt_bundle_attach(void *config);
enum { ERR_TCP_TRANSPORT_CONNECTION_TIMEOUT = 0 };
typedef struct fake_transport *esp_transport_handle_t;
typedef int (*connect_fn)(esp_transport_handle_t, const char *, int, int);
typedef int (*read_fn)(esp_transport_handle_t, char *, int, int);
typedef int (*write_fn)(esp_transport_handle_t, const char *, int, int);
typedef int (*close_fn)(esp_transport_handle_t);
esp_transport_handle_t esp_transport_init(void);
esp_transport_handle_t esp_transport_ssl_init(void);
esp_err_t esp_transport_destroy(esp_transport_handle_t);
void *esp_transport_get_context_data(esp_transport_handle_t);
esp_err_t esp_transport_set_context_data(esp_transport_handle_t, void *);
esp_err_t esp_transport_set_func(esp_transport_handle_t, connect_fn, read_fn, write_fn, close_fn, void *, void *, void *);
void esp_transport_ssl_crt_bundle_attach(esp_transport_handle_t, esp_err_t (*)(void *));
int esp_transport_connect_async(esp_transport_handle_t, const char *, int, int);
int esp_transport_read(esp_transport_handle_t, char *, int, int);
int esp_transport_write(esp_transport_handle_t, const char *, int, int);
int esp_transport_close(esp_transport_handle_t);
int esp_transport_get_socket(esp_transport_handle_t);
typedef int esp_http_client_method_t;
#define HTTP_METHOD_GET 0
#define HTTP_METHOD_PUT 2
#define ESP_ERR_HTTP_EAGAIN 0x7007
#define HTTP_EVENT_ON_HEADER 1
typedef struct { void *user_data; int event_id; const char *header_key; const char *header_value; } esp_http_client_event_t;
typedef struct {
    const char *url; esp_http_client_method_t method; int timeout_ms; bool disable_auto_redirect; int max_authorization_retries;
    esp_err_t (*crt_bundle_attach)(void *); esp_err_t (*event_handler)(esp_http_client_event_t *);
    void *user_data; int buffer_size; esp_transport_handle_t transport;
} esp_http_client_config_t;
typedef struct fake_client *esp_http_client_handle_t;
esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t *);
esp_err_t esp_http_client_set_header(esp_http_client_handle_t, const char *, const char *);
esp_err_t esp_http_client_open(esp_http_client_handle_t, int);
int64_t esp_http_client_fetch_headers(esp_http_client_handle_t);
int esp_http_client_get_status_code(esp_http_client_handle_t);
int64_t esp_http_client_get_content_length(esp_http_client_handle_t);
int esp_http_client_read(esp_http_client_handle_t, char *, int);
int esp_http_client_write(esp_http_client_handle_t, const char *, int);
bool esp_http_client_is_complete_data_received(esp_http_client_handle_t);
esp_err_t esp_http_client_cleanup(esp_http_client_handle_t);
