#include "recording.h"

#include <stdlib.h>
#include <string.h>

#include "board_audio.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "recording_clip.h"
#include "recording_state.h"
#include "sdkconfig.h"

#define WORKER_NAME "recording"
/* The duration on the panel changes at most this often. */
#define VIEW_PERIOD_US INT64_C(200000)
#define US_PER_MS 1000
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

/* Placeholder until the upload exists: admission is refused and the clip is kept. */
static void run_upload(void)
{
    taskENTER_CRITICAL(&lock);
    recording_state_refused(&state);
    taskEXIT_CRITICAL(&lock);
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

static void dispatch(recording_effect_t (*command)(recording_state_t *, uint64_t), uint64_t generation)
{
    recording_view_t view;
    taskENTER_CRITICAL(&lock);
    const recording_effect_t effect = command(&state, generation);
    snapshot(&view);
    taskEXIT_CRITICAL(&lock);
    if (effect == RECORDING_EFFECT_NONE) {
        return;
    }
    publish(&view);
    if (effect == RECORDING_EFFECT_VIEW) {
        start_job();
    }
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

/* Upload, queries and discard arrive in the next steps of stage 5b. */
void recording_check(uint64_t generation)
{
    (void)generation;
}

void recording_discard(uint64_t generation)
{
    (void)generation;
}

bool recording_read_message(uint64_t generation, void (*reader)(const char *text, void *context), void *context)
{
    (void)generation;
    (void)reader;
    (void)context;
    return false;
}

esp_err_t recording_start(recording_view_fn on_view)
{
    ESP_RETURN_ON_FALSE(on_view != NULL && view_callback == NULL, ESP_ERR_INVALID_STATE, TAG, "start");
    view_callback = on_view;
    recording_state_init(&state);
    const esp_err_t error = recording_clip_init();
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "audio partition unusable: %s", esp_err_to_name(error));
        report_failed(RECORDING_FAILURE_START, false);
        return error;
    }
    publish_current();
    start_job();
    return ESP_OK;
}
