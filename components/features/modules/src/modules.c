#include "modules.h"

#include <stdlib.h>
#include <string.h>

#include "bridge_http.h"
#include "bridge_origin_store.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "memory_report.h"
#include "modules_state.h"
#include "recording.h"
#include "sdkconfig.h"

#define TAG "modules"
#define WORKER_NAME "modules"

typedef struct {
    bridge_resource_page_t page;
    char arena[BRIDGE_RESOURCE_ARENA_SIZE];
} list_slot_t;

typedef struct {
    bridge_resource_detail_t detail;
    char arena[BRIDGE_RESOURCE_ARENA_SIZE];
} detail_slot_t;

/* Guards everything below. Held for state changes and while the UI copies data out, never during I/O. */
static SemaphoreHandle_t lock;
static modules_view_fn view_callback;
static modules_state_t state;
static uint64_t revision;
static list_slot_t *list_slot;
static detail_slot_t *detail_slot;

static const char *const KIND_NAMES[BRIDGE_RESOURCE_KIND_COUNT] = {"tasks", "notes", "alerts"};

static void run(void *unused);

/* Called with the lock held. */
static void free_slots(unsigned effects)
{
    if ((effects & MODULES_EFFECT_FREE_LIST) != 0U) {
        free(list_slot);
        list_slot = NULL;
    }
    if ((effects & MODULES_EFFECT_FREE_DETAIL) != 0U) {
        free(detail_slot);
        detail_slot = NULL;
    }
}

/* Starts the worker for the request just claimed; its first request is state.running_request. */
static void start_worker(void)
{
    if (xTaskCreate(run, WORKER_NAME, CONFIG_MODULES_WORKER_STACK_SIZE, NULL, CONFIG_MODULES_WORKER_PRIORITY,
                    NULL) == pdPASS) return;
    ESP_LOGE(TAG, "worker unavailable");
    xSemaphoreTake(lock, portMAX_DELAY);
    const unsigned effects = modules_state_start_failed(&state);
    free_slots(effects);
    modules_view_t view;
    modules_state_view(&state, &view);
    view.revision = ++revision;
    xSemaphoreGive(lock);
    if ((effects & MODULES_EFFECT_VIEW) != 0U) view_callback(&view);
}

/* Takes ownership of the held lock: applies the effects, releases it, then publishes and starts work. */
static void commit(unsigned effects)
{
    free_slots(effects);
    modules_view_t view;
    modules_state_view(&state, &view);
    view.revision = (effects & MODULES_EFFECT_VIEW) != 0U ? ++revision : revision;
    const modules_request_t request = (effects & MODULES_EFFECT_WORK) != 0U ? modules_state_claim(&state)
                                                                           : (modules_request_t){0};
    xSemaphoreGive(lock);
    if ((effects & MODULES_EFFECT_VIEW) != 0U) view_callback(&view);
    if (request.type != MODULES_REQUEST_NONE) start_worker();
}

esp_err_t modules_start(modules_view_fn on_view)
{
    if (on_view == NULL) return ESP_ERR_INVALID_ARG;
    if (lock != NULL) return ESP_ERR_INVALID_STATE;
    lock = xSemaphoreCreateMutex();
    if (lock == NULL) return ESP_ERR_NO_MEM;
    modules_state_init(&state);
    view_callback = on_view;
    return ESP_OK;
}

void modules_enter(bridge_resource_kind_t kind)
{
    xSemaphoreTake(lock, portMAX_DELAY);
    commit(modules_state_enter(&state, kind));
}

void modules_leave(void)
{
    xSemaphoreTake(lock, portMAX_DELAY);
    commit(modules_state_leave(&state));
}

void modules_previous(uint64_t ticket)
{
    xSemaphoreTake(lock, portMAX_DELAY);
    commit(modules_state_previous(&state, ticket));
}

void modules_next(uint64_t ticket)
{
    xSemaphoreTake(lock, portMAX_DELAY);
    commit(modules_state_next(&state, ticket));
}

void modules_refresh(uint64_t ticket)
{
    xSemaphoreTake(lock, portMAX_DELAY);
    commit(modules_state_refresh(&state, ticket));
}

void modules_open_detail(uint64_t ticket, uint64_t id)
{
    xSemaphoreTake(lock, portMAX_DELAY);
    commit(modules_state_open_detail(&state, ticket, id));
}

void modules_back(uint64_t ticket)
{
    xSemaphoreTake(lock, portMAX_DELAY);
    commit(modules_state_back(&state, ticket));
}

bool modules_read_page(uint64_t ticket, void (*reader)(const bridge_resource_page_t *page, void *context),
                       void *context)
{
    xSemaphoreTake(lock, portMAX_DELAY);
    const bool available = modules_state_is_current(&state, ticket) && state.screen == MODULES_SCREEN_LIST &&
                           state.status == MODULES_READY && list_slot != NULL;
    if (available) reader(&list_slot->page, context);
    xSemaphoreGive(lock);
    return available;
}

bool modules_read_detail(uint64_t ticket, void (*reader)(const bridge_resource_detail_t *detail, void *context),
                         void *context)
{
    xSemaphoreTake(lock, portMAX_DELAY);
    const bool available = modules_state_is_current(&state, ticket) && state.screen == MODULES_SCREEN_DETAIL &&
                           state.status == MODULES_READY && detail_slot != NULL;
    if (available) reader(&detail_slot->detail, context);
    xSemaphoreGive(lock);
    return available;
}

static modules_error_t transport_error(const bridge_http_response_t *response)
{
    switch (response->result) {
    case BRIDGE_HTTP_BUSY: return MODULES_ERROR_BUSY_BRIDGE;
    case BRIDGE_HTTP_NO_MEMORY: return MODULES_ERROR_NO_MEMORY;
    case BRIDGE_HTTP_TOO_LARGE: return MODULES_ERROR_TOO_LARGE;
    case BRIDGE_HTTP_CLOCK_NOT_READY:
    case BRIDGE_HTTP_TRANSPORT_ERROR:
    case BRIDGE_HTTP_TIMEOUT: return MODULES_ERROR_NETWORK;
    default: return MODULES_ERROR_INVALID;
    }
}

static modules_error_t decode_error(bridge_resource_result_t result)
{
    switch (result) {
    case BRIDGE_RESOURCE_OK: return MODULES_ERROR_NONE;
    case BRIDGE_RESOURCE_TOO_LARGE: return MODULES_ERROR_TOO_LARGE;
    case BRIDGE_RESOURCE_NOT_FOUND: return MODULES_ERROR_NOT_FOUND;
    case BRIDGE_RESOURCE_UNAUTHORIZED: return MODULES_ERROR_UNAUTHORIZED;
    case BRIDGE_RESOURCE_DENIED: return MODULES_ERROR_DENIED;
    case BRIDGE_RESOURCE_UNAVAILABLE: return MODULES_ERROR_UNAVAILABLE;
    case BRIDGE_RESOURCE_REJECTED: return MODULES_ERROR_REJECTED;
    default: return MODULES_ERROR_INVALID;
    }
}

/* One GET. On a response, body and its length are valid; otherwise the error says why there is none. */
static modules_error_t fetch(const char *path, char *body, bridge_http_response_t *response)
{
    if (recording_reads_blocked()) return MODULES_ERROR_BUSY_AUDIO;
    bridge_origin_t origin;
    if (bridge_origin_load(&origin) != ESP_OK) {
        memset(&origin, 0, sizeof(origin));
        return MODULES_ERROR_NETWORK;
    }
    *response = bridge_http_get(&origin, path, BRIDGE_RESOURCE_GATE_WAIT_MS, BRIDGE_RESOURCE_IO_MS, body,
                                BRIDGE_RESOURCE_BODY_MAX_BYTES);
    memset(&origin, 0, sizeof(origin));
    return response->result == BRIDGE_HTTP_OK ? MODULES_ERROR_NONE : transport_error(response);
}

static void deliver_list(const modules_request_t *request, modules_error_t error, list_slot_t *slot)
{
    modules_page_t info = {0};
    if (error == MODULES_ERROR_NONE) {
        info.page = slot->page.page;
        info.has_previous = slot->page.has_previous;
        info.has_next = slot->page.has_next;
        info.count = slot->page.count;
        for (uint8_t i = 0; i < slot->page.count; ++i) info.ids[i] = slot->page.entries[i].id;
    }
    xSemaphoreTake(lock, portMAX_DELAY);
    const unsigned effects = modules_state_list_done(&state, request->ticket, error, error == MODULES_ERROR_NONE ? &info : NULL);
    if ((effects & MODULES_EFFECT_ACCEPT) != 0U) {
        free(list_slot);
        list_slot = slot;
        slot = NULL;
    }
    free(slot);
    commit(effects);
}

static void deliver_detail(const modules_request_t *request, modules_error_t error, detail_slot_t *slot)
{
    xSemaphoreTake(lock, portMAX_DELAY);
    const unsigned effects = modules_state_detail_done(&state, request->ticket, error);
    if ((effects & MODULES_EFFECT_ACCEPT) != 0U) {
        free(detail_slot);
        detail_slot = slot;
        slot = NULL;
    }
    free(slot);
    commit(effects);
}

/* The transport result of a request that was sent; NO_TRANSPORT_RESULT when the read failed before sending. */
#define NO_TRANSPORT_RESULT (-1)

static int transport_result(const bridge_http_response_t *response)
{
    return response->result == BRIDGE_HTTP_OK ? NO_TRANSPORT_RESULT : (int)response->result;
}

static void read_list(const modules_request_t *request)
{
    char path[BRIDGE_RESOURCE_PATH_SIZE];
    char *body = calloc(1U, BRIDGE_RESOURCE_BODY_MAX_BYTES);
    list_slot_t *slot = calloc(1U, sizeof(*slot));
    bridge_http_response_t response = {0};
    modules_error_t error = MODULES_ERROR_NO_MEMORY;
    if (body != NULL && slot != NULL) {
        error = bridge_resource_list_path(request->kind, request->number, path, sizeof(path)) ? MODULES_ERROR_NONE
                                                                                              : MODULES_ERROR_INVALID;
        if (error == MODULES_ERROR_NONE) error = fetch(path, body, &response);
        if (error == MODULES_ERROR_NONE) {
            error = decode_error(bridge_resource_decode_page(request->kind, request->number, response.status, body,
                                                             response.length, slot->arena, sizeof(slot->arena),
                                                             &slot->page));
            ESP_LOGI(TAG, "%s page %llu: HTTP %d, %u bytes, error %d", KIND_NAMES[request->kind],
                     (unsigned long long)request->number, response.status, (unsigned)response.length, (int)error);
        } else {
            ESP_LOGW(TAG, "%s page %llu: error %d (transport %d)", KIND_NAMES[request->kind],
                     (unsigned long long)request->number, (int)error, transport_result(&response));
        }
    }
    free(body);
    deliver_list(request, error, slot);
}

static void read_detail(const modules_request_t *request)
{
    char path[BRIDGE_RESOURCE_PATH_SIZE];
    char *body = calloc(1U, BRIDGE_RESOURCE_BODY_MAX_BYTES);
    detail_slot_t *slot = calloc(1U, sizeof(*slot));
    bridge_http_response_t response = {0};
    modules_error_t error = MODULES_ERROR_NO_MEMORY;
    if (body != NULL && slot != NULL) {
        error = bridge_resource_detail_path(request->kind, request->number, path, sizeof(path)) ? MODULES_ERROR_NONE
                                                                                                : MODULES_ERROR_INVALID;
        if (error == MODULES_ERROR_NONE) error = fetch(path, body, &response);
        if (error == MODULES_ERROR_NONE) {
            error = decode_error(bridge_resource_decode_detail(request->kind, request->number, response.status, body,
                                                               response.length, slot->arena, sizeof(slot->arena),
                                                               &slot->detail));
            ESP_LOGI(TAG, "%s detail: HTTP %d, %u bytes, error %d", KIND_NAMES[request->kind], response.status,
                     (unsigned)response.length, (int)error);
        } else {
            ESP_LOGW(TAG, "%s detail: error %d (transport %d)", KIND_NAMES[request->kind], (int)error,
                     transport_result(&response));
        }
    }
    free(body);
    deliver_detail(request, error, slot);
}

static bool request_is_current(const modules_request_t *request)
{
    xSemaphoreTake(lock, portMAX_DELAY);
    const bool current = modules_state_is_current(&state, request->ticket);
    xSemaphoreGive(lock);
    return current;
}

static void run(void *unused)
{
    (void)unused;
    xSemaphoreTake(lock, portMAX_DELAY);
    modules_request_t request = state.running_request;
    xSemaphoreGive(lock);
    while (request.type != MODULES_REQUEST_NONE) {
        if (request_is_current(&request)) {
            if (request.type == MODULES_REQUEST_LIST) read_list(&request);
            else read_detail(&request);
            memory_report_log("modules read");
        }
        xSemaphoreTake(lock, portMAX_DELAY);
        request = modules_state_finish(&state);
        xSemaphoreGive(lock);
    }
    vTaskDelete(NULL);
}
