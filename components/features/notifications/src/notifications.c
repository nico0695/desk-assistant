#include "notifications.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

#include "bridge_events.h"
#include "bridge_http.h"
#include "bridge_origin_store.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "notification_journal.h"
#include "notification_state.h"
#include "sdkconfig.h"

#define WORKER_NAME "notifications"
#define ACK_PATH_SIZE (sizeof(BRIDGE_EVENT_ACK_PATH_FORMAT) + BRIDGE_UUID_LENGTH)
#define COMMAND_QUEUE_LENGTH 8U

typedef struct {
    char event_id[BRIDGE_UUID_SIZE];
} close_command_t;

static const char *TAG = "notifications";
static notification_state_t state;
static notification_view_fn view_callback;
static QueueHandle_t commands;
static esp_timer_handle_t retry_timer;
static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
static TaskHandle_t worker;
static bool starting;
static bool drain_requested;
static bool waiting_gate;
static bool stream_online;
static bool origin_held;
static atomic_bool uncertain;
static uint32_t gate_epoch;
static uint64_t generation;
static uint64_t arrival_sequence;
static notification_view_notice_t notice;

static void publish(void)
{
    if (view_callback == NULL) return;
    taskENTER_CRITICAL(&lock);
    const uint64_t current_generation = ++generation;
    taskEXIT_CRITICAL(&lock);
    notification_view_t view = {.generation = current_generation, .arrival_sequence = arrival_sequence,
                                .count = state.count, .notice = notice};
    if (state.count != 0U) view.card = state.cards[0];
    view_callback(&view);
}

static void run(void *unused);

static void schedule(bool drain)
{
    TaskHandle_t active;
    bool create = false;
    taskENTER_CRITICAL(&lock);
    if (drain) drain_requested = true;
    active = worker;
    if (active == NULL && !starting) {
        starting = true;
        create = true;
    }
    taskEXIT_CRITICAL(&lock);
    if (active != NULL) xTaskNotifyGive(active);
    if (!create) return;
    TaskHandle_t created = NULL;
    if (xTaskCreate(run, WORKER_NAME, CONFIG_NOTIFICATIONS_WORKER_STACK_SIZE, NULL,
                    CONFIG_NOTIFICATIONS_WORKER_PRIORITY, &created) != pdPASS) {
        taskENTER_CRITICAL(&lock);
        starting = false;
        taskEXIT_CRITICAL(&lock);
        ESP_LOGE(TAG, "worker unavailable");
        return;
    }
    taskENTER_CRITICAL(&lock);
    worker = created;
    starting = false;
    taskEXIT_CRITICAL(&lock);
    xTaskNotifyGive(created);
}

static void retry_timer_fired(void *unused)
{
    (void)unused;
    schedule(true);
}

static void retry_ack_later(void)
{
    if (retry_timer != NULL) {
        (void)esp_timer_stop(retry_timer);
        (void)esp_timer_start_once(retry_timer, (uint64_t)CONFIG_NOTIFICATIONS_ACK_RETRY_MS * 1000U);
    }
}

static bool hold_origin(void)
{
    if (origin_held) return true;
    if (!bridge_origin_hold()) return false;
    origin_held = true;
    return true;
}

static void release_idle_origin(void)
{
    if (origin_held && !uncertain && !notification_state_has_work(&state)) {
        bridge_origin_release();
        origin_held = false;
    }
}

static bool save_candidate(notification_state_t *candidate)
{
    const esp_err_t error = notification_journal_save(candidate);
    if (error != ESP_OK) {
        /* The commit may have reached flash. Keep the origin and block until reboot. */
        uncertain = true;
        notice = NOTIFICATION_VIEW_STORAGE_ERROR;
        ESP_LOGE(TAG, "journal commit failed: %s", esp_err_to_name(error));
        publish();
        return false;
    }
    state = *candidate;
    notice = NOTIFICATION_VIEW_OK;
    publish();
    return true;
}

static notification_state_t *candidate_copy(void)
{
    notification_state_t *candidate = malloc(sizeof(*candidate));
    if (candidate != NULL) *candidate = state;
    else ESP_LOGE(TAG, "no memory for journal transition");
    return candidate;
}

static bool close_front(const char *event_id)
{
    notification_state_t *candidate = candidate_copy();
    if (candidate == NULL) {
        notice = NOTIFICATION_VIEW_STORAGE_ERROR;
        publish();
        return false;
    }
    const bool changed = notification_state_close(candidate, event_id);
    const bool saved = changed && save_candidate(candidate);
    free(candidate);
    if (saved) {
        release_idle_origin();
        schedule(true);
    }
    return saved;
}

static bridge_http_response_t send_ack(const char *id)
{
    bridge_http_response_t response = {.result = BRIDGE_HTTP_INVALID_ARGUMENT};
    char path[ACK_PATH_SIZE];
    if (snprintf(path, sizeof(path), BRIDGE_EVENT_ACK_PATH_FORMAT, id) >= (int)sizeof(path)) return response;
    bridge_origin_t origin;
    if (bridge_origin_load(&origin) != ESP_OK) return response;
    char body[1];
    response = bridge_http_json(&origin, path, BRIDGE_HTTP_PUT_JSON, BRIDGE_EVENT_ACK_BODY,
                                strlen(BRIDGE_EVENT_ACK_BODY), BRIDGE_HTTP_EVENT_GATE_MS,
                                BRIDGE_HTTP_EVENT_IO_MS, body, sizeof(body));
    memset(&origin, 0, sizeof(origin));
    return response;
}

static void handle_busy(uint32_t before)
{
    taskENTER_CRITICAL(&lock);
    if (gate_epoch == before) {
        waiting_gate = true;
        drain_requested = false;
    } else drain_requested = true;
    taskEXIT_CRITICAL(&lock);
}

static bool acknowledge(void)
{
    char id[BRIDGE_UUID_SIZE];
    memcpy(id, state.pending_ack, sizeof(id));
    uint32_t before;
    taskENTER_CRITICAL(&lock);
    before = gate_epoch;
    taskEXIT_CRITICAL(&lock);
    bridge_http_response_t response = send_ack(id);
    if (response.result == BRIDGE_HTTP_BUSY) {
        handle_busy(before);
        return false;
    }
    if (response.result != BRIDGE_HTTP_OK || !bridge_event_ack_succeeded(response.status, response.length)) {
        ESP_LOGW(TAG, "ACK pending: transport %d HTTP %d", response.result, response.status);
        taskENTER_CRITICAL(&lock);
        drain_requested = false;
        taskEXIT_CRITICAL(&lock);
        retry_ack_later();
        return false;
    }
    notification_state_t *candidate = candidate_copy();
    if (candidate == NULL) {
        retry_ack_later();
        return false;
    }
    const bool changed = notification_state_acknowledged(candidate, id);
    const bool saved = changed && save_candidate(candidate);
    free(candidate);
    if (saved) release_idle_origin();
    return saved;
}

static bool receive_one(void)
{
    if (!hold_origin()) return false;
    bridge_origin_t origin;
    if (bridge_origin_load(&origin) != ESP_OK) {
        release_idle_origin();
        return false;
    }
    char *body = malloc(BRIDGE_EVENT_RESPONSE_MAX_BYTES + 1U);
    if (body == NULL) {
        memset(&origin, 0, sizeof(origin));
        release_idle_origin();
        return false;
    }
    uint32_t before;
    taskENTER_CRITICAL(&lock);
    before = gate_epoch;
    taskEXIT_CRITICAL(&lock);
    bridge_http_response_t response = bridge_http_get(&origin, BRIDGE_EVENT_GET_PATH,
        BRIDGE_HTTP_EVENT_GATE_MS, BRIDGE_HTTP_EVENT_IO_MS, body, BRIDGE_EVENT_RESPONSE_MAX_BYTES);
    memset(&origin, 0, sizeof(origin));
    if (response.result == BRIDGE_HTTP_BUSY) {
        free(body);
        handle_busy(before);
        release_idle_origin();
        return false;
    }
    bridge_event_t event;
    bool present = false;
    bridge_event_decode_t decoded = response.result == BRIDGE_HTTP_OK
        ? bridge_event_decode(body, response.length, response.status, &event, &present) : BRIDGE_EVENT_DECODE_INVALID;
    free(body);
    if (decoded != BRIDGE_EVENT_DECODE_OK) {
        ESP_LOGW(TAG, "event read failed: transport %d HTTP %d decode %d", response.result, response.status, decoded);
        taskENTER_CRITICAL(&lock);
        drain_requested = false;
        taskEXIT_CRITICAL(&lock);
        release_idle_origin();
        return false;
    }
    if (!present) {
        taskENTER_CRITICAL(&lock);
        drain_requested = false;
        taskEXIT_CRITICAL(&lock);
        release_idle_origin();
        return false;
    }
    notification_state_t *candidate = candidate_copy();
    if (candidate == NULL) {
        release_idle_origin();
        return false;
    }
    const notification_receive_t received = notification_state_receive(candidate, &event);
    bool saved = true;
    if (received == NOTIFICATION_RECEIVED ||
        (received == NOTIFICATION_DUPLICATE && state.pending_ack[0] == '\0')) {
        saved = save_candidate(candidate);
        if (saved && received == NOTIFICATION_RECEIVED) {
            ++arrival_sequence;
            publish();
        }
    }
    free(candidate);
    if (!saved || received == NOTIFICATION_INVALID || received == NOTIFICATION_FULL ||
        received == NOTIFICATION_ACK_PENDING) {
        release_idle_origin();
        return false;
    }
    return true;
}

static void run(void *unused)
{
    (void)unused;
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    for (;;) {
        close_command_t command;
        while (!uncertain && xQueueReceive(commands, &command, 0) == pdTRUE) (void)close_front(command.event_id);
        if (uncertain) {
            taskENTER_CRITICAL(&lock);
            worker = NULL;
            taskEXIT_CRITICAL(&lock);
            break;
        }
        if (state.pending_ack[0] != '\0') {
            if (acknowledge()) continue;
        } else {
            bool fetch;
            taskENTER_CRITICAL(&lock);
            fetch = drain_requested && stream_online && state.count < BRIDGE_EVENT_QUEUE_CAPACITY;
            if (fetch) drain_requested = false;
            if (!fetch && state.count == BRIDGE_EVENT_QUEUE_CAPACITY) drain_requested = false;
            taskEXIT_CRITICAL(&lock);
            if (fetch && receive_one()) {
                taskENTER_CRITICAL(&lock);
                drain_requested = true;
                taskEXIT_CRITICAL(&lock);
                continue;
            }
        }
        const bool queued = uxQueueMessagesWaiting(commands) != 0U;
        taskENTER_CRITICAL(&lock);
        const bool again = queued || (drain_requested && !waiting_gate && !uncertain && stream_online &&
                                      state.count < BRIDGE_EVENT_QUEUE_CAPACITY);
        if (!again) worker = NULL;
        taskEXIT_CRITICAL(&lock);
        if (!again) break;
    }
    vTaskDelete(NULL);
}

esp_err_t notifications_start(notification_view_fn on_view)
{
    if (on_view == NULL || view_callback != NULL) return ESP_ERR_INVALID_ARG;
    commands = xQueueCreate(COMMAND_QUEUE_LENGTH, sizeof(close_command_t));
    if (commands == NULL) return ESP_ERR_NO_MEM;
    const esp_timer_create_args_t timer = {.callback = retry_timer_fired, .name = "notification_ack"};
    if (esp_timer_create(&timer, &retry_timer) != ESP_OK) {
        vQueueDelete(commands);
        commands = NULL;
        return ESP_ERR_NO_MEM;
    }
    notification_state_init(&state);
    const esp_err_t error = notification_journal_load(&state);
    uncertain = error != ESP_OK;
    notice = uncertain ? NOTIFICATION_VIEW_STORAGE_ERROR : NOTIFICATION_VIEW_OK;
    if ((uncertain || notification_state_has_work(&state)) && !bridge_origin_hold()) {
        esp_timer_delete(retry_timer);
        retry_timer = NULL;
        vQueueDelete(commands);
        commands = NULL;
        return ESP_ERR_INVALID_STATE;
    }
    origin_held = uncertain || notification_state_has_work(&state);
    view_callback = on_view;
    bridge_http_set_gate_released(notifications_gate_released);
    publish();
    if (!uncertain && state.pending_ack[0] != '\0') schedule(true);
    return ESP_OK;
}

void notifications_pending_events(void) { if (!uncertain) schedule(true); }

void notifications_bridge_online(bool online)
{
    taskENTER_CRITICAL(&lock);
    stream_online = online;
    taskEXIT_CRITICAL(&lock);
    if (online && !uncertain) schedule(true);
}

void notifications_gate_released(void)
{
    bool wake;
    taskENTER_CRITICAL(&lock);
    ++gate_epoch;
    wake = waiting_gate;
    if (wake) waiting_gate = false;
    taskEXIT_CRITICAL(&lock);
    if (wake && !uncertain) schedule(true);
}

void notifications_close(uint64_t expected_generation, const char *event_id)
{
    taskENTER_CRITICAL(&lock);
    const bool current = expected_generation != 0U && expected_generation <= generation;
    taskEXIT_CRITICAL(&lock);
    if (commands == NULL || !current || !bridge_uuid_valid(event_id)) return;
    close_command_t command;
    memcpy(command.event_id, event_id, sizeof(command.event_id));
    if (xQueueSend(commands, &command, 0) == pdTRUE) schedule(true);
}
