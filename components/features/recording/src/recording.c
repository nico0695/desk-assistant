#include "recording.h"

#include <stdlib.h>
#include <string.h>

#include "board_audio.h"
#include "bridge.h"
#include "bridge_assistant.h"
#include "bridge_http.h"
#include "bridge_origin_store.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "memory_report.h"
#include "recording_clip.h"
#include "recording_record.h"
#include "recording_state.h"
#include "sdkconfig.h"

#define WORKER_NAME "recording"
/* The duration on the panel changes at most this often. */
#define VIEW_PERIOD_US INT64_C(200000)
#define US_PER_MS 1000
#define UUID_RANDOM_BYTES 16U
#define WARMUP_SAMPLES (BOARD_AUDIO_SAMPLE_RATE_HZ * CONFIG_RECORDING_WARMUP_MS / 1000U)
#define CAPTURE_GUARD_US ((int64_t)(RECORDING_MAX_MS + CONFIG_RECORDING_DURATION_GUARD_MS) * US_PER_MS)

_Static_assert(BOARD_AUDIO_SAMPLE_RATE_HZ == RECORDING_SAMPLE_RATE_HZ, "capture rate differs from the contract");

/* Technical figures only, logged once per capture; never audio. */
typedef struct {
    uint32_t peak;
    uint64_t absolute_sum;
    int64_t slowest_write_us;
} capture_metrics_t;

static const char *TAG = "recording";
static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
/* Guarded by lock. The state runs one job at a time, so two workers never touch the hardware or the record. */
static recording_state_t state;
/* Guarded by lock; numbers every view so the consumer can drop one that arrives late. */
static uint64_t view_revision;
/* Guarded by lock: the latest event stream state, for admission and queries. */
static bool stream_online;
static uint64_t stream_generation;
/* Guarded by lock: the generation the armed local wait belongs to. */
static uint64_t wait_generation;
static esp_timer_handle_t wait_timer;
/*
 * The bridge message of the shown terminal, guarded by message_lock: heap, owned here from its reply until the attempt
 * ends; readers copy it. A mutex rather than lock because a reader copies up to 8 KB.
 */
static SemaphoreHandle_t message_lock;
static char *message;
static uint64_t message_generation;
/* Only the worker (or startup, before any worker) touches it: the origin is held for the attempt. */
static bool origin_held;
static recording_view_fn view_callback;

/* Call under lock. */
static void snapshot(recording_view_t *view)
{
    recording_state_view(&state, view);
    view->revision = ++view_revision;
}

static void publish(const recording_view_t *view)
{
    view_callback(view);
}

static void publish_current(void)
{
    recording_view_t view;
    taskENTER_CRITICAL(&lock);
    snapshot(&view);
    taskEXIT_CRITICAL(&lock);
    publish(&view);
}

static recording_phase_t current_phase(void)
{
    taskENTER_CRITICAL(&lock);
    const recording_phase_t phase = state.phase;
    taskEXIT_CRITICAL(&lock);
    return phase;
}

static void report_cleaned(bool cleaned)
{
    taskENTER_CRITICAL(&lock);
    recording_state_cleaned(&state, cleaned);
    taskEXIT_CRITICAL(&lock);
    publish_current();
}

/* Returns true when Send was already tapped and the upload follows in this job. */
static bool report_ready(void)
{
    taskENTER_CRITICAL(&lock);
    const bool upload = recording_state_ready(&state);
    taskEXIT_CRITICAL(&lock);
    publish_current();
    return upload;
}

static void report_failed(recording_failure_t failure, bool cleaned)
{
    taskENTER_CRITICAL(&lock);
    recording_state_failed(&state, failure, cleaned);
    taskEXIT_CRITICAL(&lock);
    publish_current();
}

static bool clear_clip(void)
{
    const int64_t started = esp_timer_get_time();
    const esp_err_t error = recording_clip_clear();
    ESP_LOGI(TAG, "clip cleared: %s in %lld ms", esp_err_to_name(error),
             (long long)((esp_timer_get_time() - started) / US_PER_MS));
    return error == ESP_OK;
}

/* Releases the microphone and clears the clip; both are attempted even if the first fails. */
static bool release_and_clear(void)
{
    const esp_err_t stopped = board_audio_stop();
    if (stopped != ESP_OK) {
        ESP_LOGE(TAG, "audio stop: %s", esp_err_to_name(stopped));
    }
    const bool cleared = clear_clip();
    return stopped == ESP_OK && cleared;
}

static void log_capture(uint32_t samples, const capture_metrics_t *metrics)
{
    ESP_LOGI(TAG, "capture: samples=%lu duration_ms=%lu overruns=%lu peak=%lu mean_abs=%lu slowest_write_ms=%lld",
             (unsigned long)samples, (unsigned long)(samples * 1000ULL / RECORDING_SAMPLE_RATE_HZ),
             (unsigned long)board_audio_overruns(), (unsigned long)metrics->peak,
             (unsigned long)(samples != 0U ? metrics->absolute_sum / samples : 0U),
             (long long)(metrics->slowest_write_us / US_PER_MS));
}

/* Returns true when the valid clip continues to the upload. */
static bool finalize(uint32_t samples, const capture_metrics_t *metrics)
{
    const esp_err_t stopped = board_audio_stop();
    log_capture(samples, metrics);
    if (stopped != ESP_OK) {
        ESP_LOGE(TAG, "audio stop: %s", esp_err_to_name(stopped));
        report_failed(RECORDING_FAILURE_CLEANUP, false);
        return false;
    }
    taskENTER_CRITICAL(&lock);
    recording_failure_t failure = recording_state_validate(&state, board_audio_overruns());
    taskEXIT_CRITICAL(&lock);
    if (failure == RECORDING_FAILURE_NONE && recording_clip_finish() != ESP_OK) {
        failure = RECORDING_FAILURE_STORAGE;
    }
    if (failure != RECORDING_FAILURE_NONE) {
        report_failed(failure, clear_clip());
        return false;
    }
    return report_ready();
}

static void measure(const int16_t *samples, size_t count, capture_metrics_t *metrics)
{
    for (size_t i = 0; i < count; ++i) {
        const uint32_t magnitude = (uint32_t)abs((int)samples[i]);
        metrics->absolute_sum += magnitude;
        if (magnitude > metrics->peak) metrics->peak = magnitude;
    }
}

/* Reads and drops the settling audio; false when the microphone does not deliver it. */
static bool discard_warmup(int16_t *block)
{
    for (uint32_t dropped = 0; dropped < WARMUP_SAMPLES;) {
        size_t count = 0;
        const size_t wanted = WARMUP_SAMPLES - dropped < BOARD_AUDIO_BLOCK_SAMPLES ? WARMUP_SAMPLES - dropped
                                                                                    : BOARD_AUDIO_BLOCK_SAMPLES;
        if (board_audio_read(block, wanted, &count, CONFIG_RECORDING_READ_TIMEOUT_MS) != ESP_OK || count == 0U) {
            return false;
        }
        dropped += (uint32_t)count;
    }
    return true;
}

/* Returns true when the valid clip continues to the upload. */
static bool run_capture(void)
{
    int16_t block[BOARD_AUDIO_BLOCK_SAMPLES];
    capture_metrics_t metrics = {0};
    uint32_t samples = 0;
    esp_err_t error = recording_clip_begin();
    if (error == ESP_OK) {
        error = board_audio_start();
    }
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "capture start: %s", esp_err_to_name(error));
        report_failed(RECORDING_FAILURE_START, release_and_clear());
        return false;
    }
    const int64_t started = esp_timer_get_time();
    int64_t published = started;
    recording_failure_t failure = discard_warmup(block) ? RECORDING_FAILURE_NONE : RECORDING_FAILURE_INTERRUPTED;
    while (failure == RECORDING_FAILURE_NONE) {
        const recording_phase_t phase = current_phase();
        if (phase == RECORDING_CANCELLING) {
            memset(block, 0, sizeof(block));
            log_capture(samples, &metrics);
            report_cleaned(release_and_clear());
            return false;
        }
        if (phase == RECORDING_FINALIZING) {
            memset(block, 0, sizeof(block));
            return finalize(samples, &metrics);
        }
        size_t count = 0;
        const size_t room = RECORDING_MAX_SAMPLES - samples;
        error = board_audio_read(block, room < BOARD_AUDIO_BLOCK_SAMPLES ? room : BOARD_AUDIO_BLOCK_SAMPLES, &count,
                                 CONFIG_RECORDING_READ_TIMEOUT_MS);
        const int64_t now = esp_timer_get_time();
        if (error != ESP_OK || count == 0U || board_audio_overruns() != 0U || now - started > CAPTURE_GUARD_US) {
            failure = RECORDING_FAILURE_INTERRUPTED;
            break;
        }
        measure(block, count, &metrics);
        const int64_t write_started = esp_timer_get_time();
        if (recording_clip_append(block, count) != ESP_OK) {
            failure = RECORDING_FAILURE_STORAGE;
            break;
        }
        const int64_t write_us = esp_timer_get_time() - write_started;
        if (write_us > metrics.slowest_write_us) metrics.slowest_write_us = write_us;
        samples += (uint32_t)count;

        recording_view_t view;
        taskENTER_CRITICAL(&lock);
        const bool limit = recording_state_captured(&state, samples);
        snapshot(&view);
        taskEXIT_CRITICAL(&lock);
        if (limit || now - published >= VIEW_PERIOD_US) {
            published = now;
            publish(&view);
        }
    }
    memset(block, 0, sizeof(block));
    log_capture(samples, &metrics);
    ESP_LOGW(TAG, "capture failed: %d", (int)failure);
    report_failed(failure, release_and_clear());
    return false;
}

/* Clears the clip after the upload and records the outcome in the state; no view of its own. */
static void clean_after_upload(void)
{
    const bool cleaned = clear_clip();
    taskENTER_CRITICAL(&lock);
    recording_state_cleaned(&state, cleaned);
    taskEXIT_CRITICAL(&lock);
}

static void hold_origin(void)
{
    origin_held = bridge_origin_hold();
    if (!origin_held) {
        ESP_LOGE(TAG, "origin hold failed");
    }
}

static void release_origin(void)
{
    if (origin_held) {
        bridge_origin_release();
        origin_held = false;
    }
}

static void drop_message(void)
{
    xSemaphoreTake(message_lock, portMAX_DELAY);
    free(message);
    message = NULL;
    message_generation = 0;
    xSemaphoreGive(message_lock);
}

/* The attempt and its durable record ended: nothing binds the origin or the event stream to it anymore. */
static void attempt_ended(void)
{
    bridge_watch_assistant(NULL);
    release_origin();
    drop_message();
}

static void clear_secret(void *buffer, size_t size)
{
    volatile unsigned char *at = buffer;
    while (size-- > 0U) *at++ = 0;
}

static bool load_origin(bridge_origin_t *origin)
{
    const esp_err_t error = bridge_origin_load(origin);
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "origin load: %s", esp_err_to_name(error));
    }
    return error == ESP_OK;
}

static void arm_wait(uint64_t generation)
{
    taskENTER_CRITICAL(&lock);
    wait_generation = generation;
    taskEXIT_CRITICAL(&lock);
    esp_timer_stop(wait_timer);
    const esp_err_t error = esp_timer_start_once(wait_timer, (uint64_t)BRIDGE_ASSISTANT_WAIT_MS * US_PER_MS);
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "wait timer: %s", esp_err_to_name(error));
    }
}

static recording_reply_t reply_of(bridge_assistant_status_t status)
{
    switch (status) {
    case BRIDGE_ASSISTANT_COMPLETED: return RECORDING_REPLY_COMPLETED;
    case BRIDGE_ASSISTANT_FAILED: return RECORDING_REPLY_FAILED;
    case BRIDGE_ASSISTANT_UNKNOWN: return RECORDING_REPLY_UNKNOWN;
    case BRIDGE_ASSISTANT_PROCESSING:
    default: return RECORDING_REPLY_PROCESSING;
    }
}

static recording_record_phase_t record_phase_of(recording_reply_t reply)
{
    switch (reply) {
    case RECORDING_REPLY_COMPLETED: return RECORDING_RECORD_COMPLETED;
    case RECORDING_REPLY_FAILED: return RECORDING_RECORD_FAILED;
    case RECORDING_REPLY_UNKNOWN: return RECORDING_RECORD_UNKNOWN;
    case RECORDING_REPLY_PROCESSING:
    default: return RECORDING_RECORD_PENDING;
    }
}

/* Shows a persisted terminal and takes ownership of its text. */
static void show_terminal(const char *id, recording_reply_t reply, bool omitted, char *text)
{
    recording_view_t view;
    xSemaphoreTake(message_lock, portMAX_DELAY);
    taskENTER_CRITICAL(&lock);
    recording_state_resolved(&state, id, reply, omitted);
    const bool shown = state.has_message && state.generation != message_generation;
    snapshot(&view);
    taskEXIT_CRITICAL(&lock);
    if (shown) {
        free(message);
        message = text;
        message_generation = view.generation;
        text = NULL;
    }
    xSemaphoreGive(message_lock);
    free(text);
    publish(&view);
}

/*
 * Applies a reply body for the attempt; a terminal is persisted before it is shown. Returns false when the body is not
 * a valid reply for this attempt, which leaves the state unchanged.
 */
static bool apply_reply(const char *id, const char *body, bridge_http_response_t response, bool upload)
{
    char *text = malloc(BRIDGE_ASSISTANT_MESSAGE_SIZE);
    if (text == NULL) {
        ESP_LOGE(TAG, "no memory for the reply");
        return false;
    }
    bridge_assistant_result_t result;
    const bridge_assistant_decode_t decoded = bridge_assistant_decode(
        body, response.length, response.status, upload, id, &result, text, BRIDGE_ASSISTANT_MESSAGE_SIZE);
    if (decoded != BRIDGE_ASSISTANT_DECODE_OK) {
        ESP_LOGW(TAG, "reply rejected: decode %d HTTP %d", (int)decoded, response.status);
        free(text);
        return false;
    }
    const recording_reply_t reply = reply_of(result.status);
    taskENTER_CRITICAL(&lock);
    const recording_apply_t apply = recording_state_reply(&state, id, reply);
    const uint64_t generation = state.generation;
    taskEXIT_CRITICAL(&lock);
    ESP_LOGI(TAG, "reply: status %d apply %d", (int)result.status, (int)apply);
    if (apply == RECORDING_APPLY_STORE && recording_record_finish(id, record_phase_of(reply)) != ESP_OK) {
        ESP_LOGE(TAG, "terminal phase not stored");
        taskENTER_CRITICAL(&lock);
        recording_state_store_failed(&state, id);
        taskEXIT_CRITICAL(&lock);
        free(text);
        publish_current();
        return true;
    }
    if (apply == RECORDING_APPLY_STORE || apply == RECORDING_APPLY_SHOW) {
        show_terminal(id, reply, result.content_omitted, text);
        return true;
    }
    free(text);
    if (apply == RECORDING_APPLY_WAIT) {
        arm_wait(generation);
        publish_current();
    }
    return true;
}

static bool read_clip(size_t offset, void *buffer, size_t capacity, void *unused)
{
    (void)unused;
    size_t count = 0;
    return recording_clip_read(offset, buffer, capacity, &count) == ESP_OK && count == capacity;
}

/* Admission: the event stream is open and the origin is loaded and held for the whole attempt. */
static bool admit(bridge_origin_t *origin)
{
    taskENTER_CRITICAL(&lock);
    const bool online = stream_online;
    taskEXIT_CRITICAL(&lock);
    if (!online || !bridge_origin_hold()) {
        return false;
    }
    if (!load_origin(origin)) {
        bridge_origin_release();
        return false;
    }
    origin_held = true;
    return true;
}

/* Reserves a new identity durably; false when the attempt cannot continue. */
static bool reserve(char id[BRIDGE_UUID_SIZE])
{
    uint8_t random_bytes[UUID_RANDOM_BYTES];
    esp_fill_random(random_bytes, sizeof(random_bytes));
    bridge_uuid_format(random_bytes, id);
    bridge_watch_assistant(id);
    const esp_err_t error = recording_record_begin(id);
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "reservation not stored: %s", esp_err_to_name(error));
    }
    taskENTER_CRITICAL(&lock);
    recording_state_reserved(&state, id, error == ESP_OK);
    taskEXIT_CRITICAL(&lock);
    publish_current();
    return error == ESP_OK;
}

/* After an upload that did not return a valid reply. */
static void upload_failed(const char *id, bool unsent)
{
    recording_upload_failure_t failure = RECORDING_UPLOAD_AMBIGUOUS;
    if (unsent) {
        const esp_err_t error = recording_record_clear();
        if (error != ESP_OK) {
            ESP_LOGE(TAG, "reservation not cleared: %s", esp_err_to_name(error));
        }
        failure = error == ESP_OK ? RECORDING_UPLOAD_UNSENT : RECORDING_UPLOAD_UNSENT_KEPT;
    }
    taskENTER_CRITICAL(&lock);
    recording_state_upload_failed(&state, id, failure);
    taskEXIT_CRITICAL(&lock);
    if (failure == RECORDING_UPLOAD_UNSENT) {
        attempt_ended();
    }
    publish_current();
}

/* The single PUT of the reserved attempt; the clip is cleared whatever the outcome. */
static void send_clip(const bridge_origin_t *origin, const char *id)
{
    char path[BRIDGE_ASSISTANT_PATH_SIZE];
    char *body = malloc(BRIDGE_ASSISTANT_JSON_MAX_BYTES);
    bridge_http_upload_t upload = {.response.result = BRIDGE_HTTP_NO_MEMORY, .unsent = true};
    if (body != NULL && bridge_assistant_path(id, path, sizeof(path))) {
        const int64_t started = esp_timer_get_time();
        upload = bridge_http_put_wav(origin, path, recording_clip_wav_size(), read_clip, NULL, body,
                                     BRIDGE_ASSISTANT_JSON_MAX_BYTES);
        ESP_LOGI(TAG, "upload: result %d HTTP %d unsent %d in %lld ms", (int)upload.response.result,
                 upload.response.status, (int)upload.unsent,
                 (long long)((esp_timer_get_time() - started) / US_PER_MS));
        memory_report_log("recording upload");
    }
    clean_after_upload();
    if (upload.response.result != BRIDGE_HTTP_OK || !apply_reply(id, body, upload.response, true)) {
        upload_failed(id, upload.unsent);
    }
    free(body);
}

static void run_upload(void)
{
    bridge_origin_t origin;
    if (!admit(&origin)) {
        taskENTER_CRITICAL(&lock);
        recording_state_refused(&state);
        taskEXIT_CRITICAL(&lock);
        publish_current();
        return;
    }
    char id[BRIDGE_UUID_SIZE];
    if (reserve(id)) {
        send_clip(&origin, id);
    } else {
        clean_after_upload();
        publish_current();
    }
    clear_secret(&origin, sizeof(origin));
}

/* One GET of the attempt. Without the event stream the intent waits for its next opening, which queries again. */
static void run_query(void)
{
    char id[BRIDGE_UUID_SIZE];
    taskENTER_CRITICAL(&lock);
    const bool online = stream_online;
    memcpy(id, state.id, sizeof(id));
    taskEXIT_CRITICAL(&lock);
    char path[BRIDGE_ASSISTANT_PATH_SIZE];
    bridge_origin_t origin;
    if (!online || !bridge_assistant_path(id, path, sizeof(path)) || !load_origin(&origin)) {
        return;
    }
    char *body = malloc(BRIDGE_ASSISTANT_JSON_MAX_BYTES);
    if (body == NULL) {
        ESP_LOGE(TAG, "no memory for the query");
    } else {
        const bridge_http_response_t response = bridge_http_get(&origin, path, BRIDGE_ASSISTANT_GET_GATE_MS,
                                                                BRIDGE_ASSISTANT_GET_IO_MS, body,
                                                                BRIDGE_ASSISTANT_JSON_MAX_BYTES);
        ESP_LOGI(TAG, "query: result %d HTTP %d", (int)response.result, response.status);
        memory_report_log("recording query");
        if (response.result == BRIDGE_HTTP_OK) {
            (void)apply_reply(id, body, response, false);
        }
        free(body);
    }
    clear_secret(&origin, sizeof(origin));
}

/* Close of a terminal or a confirmed discard: the audio goes first, then the durable identity. */
static void run_forget(void)
{
    const bool cleaned = clear_clip();
    esp_err_t error = ESP_FAIL;
    if (cleaned) {
        error = recording_record_clear();
        if (error != ESP_OK) {
            ESP_LOGE(TAG, "record not cleared: %s", esp_err_to_name(error));
        }
    }
    taskENTER_CRITICAL(&lock);
    recording_state_forgotten(&state, cleaned, error == ESP_OK);
    taskEXIT_CRITICAL(&lock);
    if (error == ESP_OK) {
        attempt_ended();
    } else if (cleaned) {
        /* Blocked until restart: the reply is no longer shown. */
        drop_message();
    }
    publish_current();
}

static void run_job(recording_job_t job)
{
    switch (job) {
    case RECORDING_JOB_CAPTURE:
        if (run_capture()) {
            run_upload();
        }
        break;
    case RECORDING_JOB_CLEANUP:
        /* Also retries any hardware a failed stop left owned; with nothing owned the stop is a no-op. */
        report_cleaned(release_and_clear());
        break;
    case RECORDING_JOB_UPLOAD:
        run_upload();
        break;
    case RECORDING_JOB_QUERY:
        run_query();
        break;
    case RECORDING_JOB_FORGET:
        run_forget();
        break;
    case RECORDING_JOB_NONE:
    default:
        break;
    }
}

static void worker(void *argument)
{
    recording_job_t job = (recording_job_t)(uintptr_t)argument;
    while (job != RECORDING_JOB_NONE) {
        run_job(job);
        taskENTER_CRITICAL(&lock);
        job = recording_state_next_job(&state);
        taskEXIT_CRITICAL(&lock);
    }
    vTaskDelete(NULL);
}

/* Starts a worker for the next wanted job, unless one is already running and will take it. */
static void start_job(void)
{
    taskENTER_CRITICAL(&lock);
    const recording_job_t job = recording_state_claim_job(&state);
    taskEXIT_CRITICAL(&lock);
    if (job == RECORDING_JOB_NONE ||
        xTaskCreate(worker, WORKER_NAME, CONFIG_RECORDING_WORKER_STACK_SIZE, (void *)(uintptr_t)job,
                    CONFIG_RECORDING_WORKER_PRIORITY, NULL) == pdPASS) {
        return;
    }
    ESP_LOGE(TAG, "worker could not start");
    taskENTER_CRITICAL(&lock);
    recording_state_start_failed(&state);
    taskEXIT_CRITICAL(&lock);
    publish_current();
}

static void apply_effect(recording_effect_t effect, const recording_view_t *view)
{
    if (effect == RECORDING_EFFECT_NONE) {
        return;
    }
    publish(view);
    if (effect == RECORDING_EFFECT_VIEW) {
        start_job();
    }
}

static void dispatch(recording_effect_t (*command)(recording_state_t *, uint64_t), uint64_t generation)
{
    recording_view_t view;
    taskENTER_CRITICAL(&lock);
    const recording_effect_t effect = command(&state, generation);
    snapshot(&view);
    taskEXIT_CRITICAL(&lock);
    apply_effect(effect, &view);
}

void recording_record(uint64_t generation)
{
    dispatch(recording_state_record, generation);
}

void recording_send(uint64_t generation)
{
    dispatch(recording_state_send, generation);
}

void recording_cancel(uint64_t generation)
{
    dispatch(recording_state_cancel, generation);
}

void recording_close(uint64_t generation)
{
    dispatch(recording_state_close, generation);
}

void recording_check(uint64_t generation)
{
    dispatch(recording_state_check, generation);
}

void recording_discard(uint64_t generation)
{
    dispatch(recording_state_discard, generation);
}

/* From the esp_timer task. */
static void wait_expired(void *unused)
{
    (void)unused;
    taskENTER_CRITICAL(&lock);
    const uint64_t generation = wait_generation;
    taskEXIT_CRITICAL(&lock);
    dispatch(recording_state_wait_expired, generation);
}

static void signal_attempt(const char *id)
{
    recording_view_t view;
    taskENTER_CRITICAL(&lock);
    const recording_effect_t effect = recording_state_signal(&state, id);
    snapshot(&view);
    taskEXIT_CRITICAL(&lock);
    apply_effect(effect, &view);
}

void recording_bridge_online(bool online, uint64_t generation)
{
    taskENTER_CRITICAL(&lock);
    /* A generation that went offline never comes back online: a new stream gets a new generation. */
    const bool newer = generation > stream_generation ||
                       (generation == stream_generation && !(online && !stream_online));
    if (newer) {
        stream_generation = generation;
        stream_online = online;
    }
    taskEXIT_CRITICAL(&lock);
    if (newer && online) {
        signal_attempt(NULL);
    }
}

/* The bridge only signals the attempt it watches. */
void recording_assistant_result(void)
{
    signal_attempt(NULL);
}

bool recording_read_message(uint64_t generation, void (*reader)(const char *text, void *context), void *context)
{
    if (reader == NULL || message_lock == NULL) {
        return false;
    }
    xSemaphoreTake(message_lock, portMAX_DELAY);
    const bool available = message != NULL && message_generation == generation;
    if (available) {
        reader(message, context);
    }
    xSemaphoreGive(message_lock);
    return available;
}

/* Resumes a stored attempt; an unreadable record blocks everything. Both keep the origin held. */
static void restore_attempt(void)
{
    recording_record_t record;
    const esp_err_t error = recording_record_load(&record);
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "attempt record unusable: %s", esp_err_to_name(error));
        recording_state_restore_failed(&state);
        hold_origin();
        return;
    }
    if (record.phase == RECORDING_RECORD_NONE) {
        return;
    }
    const recording_reply_t stored = record.phase == RECORDING_RECORD_PENDING     ? RECORDING_REPLY_PROCESSING
                                     : record.phase == RECORDING_RECORD_COMPLETED ? RECORDING_REPLY_COMPLETED
                                     : record.phase == RECORDING_RECORD_FAILED    ? RECORDING_REPLY_FAILED
                                                                                  : RECORDING_REPLY_UNKNOWN;
    recording_state_restore(&state, record.id, stored);
    ESP_LOGI(TAG, "attempt restored: phase %d", (int)record.phase);
    hold_origin();
    bridge_watch_assistant(record.id);
    if (stored == RECORDING_REPLY_PROCESSING) {
        arm_wait(state.generation);
    }
}

esp_err_t recording_start(recording_view_fn on_view)
{
    ESP_RETURN_ON_FALSE(on_view != NULL && view_callback == NULL, ESP_ERR_INVALID_STATE, TAG, "start");
    message_lock = xSemaphoreCreateMutex();
    ESP_RETURN_ON_FALSE(message_lock != NULL, ESP_ERR_NO_MEM, TAG, "message lock");
    const esp_timer_create_args_t timer = {.callback = wait_expired, .name = "recording_wait"};
    ESP_RETURN_ON_ERROR(esp_timer_create(&timer, &wait_timer), TAG, "wait timer");
    view_callback = on_view;
    recording_state_init(&state);
    /* Before the clip: a stored attempt keeps its identity and origin even without an audio partition. */
    restore_attempt();
    const esp_err_t error = recording_clip_init();
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "audio partition unusable: %s", esp_err_to_name(error));
        if (state.id[0] == '\0') {
            report_failed(RECORDING_FAILURE_START, false);
            return error;
        }
    }
    publish_current();
    start_job();
    return error;
}
