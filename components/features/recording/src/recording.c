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

typedef enum { JOB_CLEANUP, JOB_CAPTURE } job_t;

/* Technical figures only, logged once per capture; never audio. */
typedef struct {
    uint32_t peak;
    uint64_t absolute_sum;
    int64_t slowest_write_us;
} capture_metrics_t;

static const char *TAG = "recording";
static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
/*
 * Guarded by lock. Only phases without a running worker (IDLE, READY_TO_SEND, FAILED) start one, and a worker
 * reports its last outcome after releasing everything, so two workers never touch the hardware at once.
 */
static recording_state_t state;
static recording_view_fn view_callback;

static void publish(const recording_view_t *view)
{
    view_callback(view);
}

static void publish_current(void)
{
    recording_view_t view;
    taskENTER_CRITICAL(&lock);
    recording_state_view(&state, &view);
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

static void report_ready(void)
{
    taskENTER_CRITICAL(&lock);
    recording_state_ready(&state);
    taskEXIT_CRITICAL(&lock);
    publish_current();
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

static void finalize(uint32_t samples, const capture_metrics_t *metrics)
{
    const esp_err_t stopped = board_audio_stop();
    log_capture(samples, metrics);
    if (stopped != ESP_OK) {
        ESP_LOGE(TAG, "audio stop: %s", esp_err_to_name(stopped));
        report_failed(RECORDING_FAILURE_CLEANUP, false);
        return;
    }
    taskENTER_CRITICAL(&lock);
    recording_failure_t failure = recording_state_validate(&state, board_audio_overruns());
    taskEXIT_CRITICAL(&lock);
    if (failure == RECORDING_FAILURE_NONE && recording_clip_finish() != ESP_OK) {
        failure = RECORDING_FAILURE_STORAGE;
    }
    if (failure != RECORDING_FAILURE_NONE) {
        report_failed(failure, clear_clip());
        return;
    }
    report_ready();
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

static void run_capture(void)
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
        return;
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
            return;
        }
        if (phase == RECORDING_FINALIZING) {
            memset(block, 0, sizeof(block));
            finalize(samples, &metrics);
            return;
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
        recording_state_view(&state, &view);
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
}

static void worker(void *argument)
{
    if ((job_t)(uintptr_t)argument == JOB_CAPTURE) {
        run_capture();
    } else {
        report_cleaned(clear_clip());
    }
    vTaskDelete(NULL);
}

static void start_worker(job_t job)
{
    if (xTaskCreate(worker, WORKER_NAME, CONFIG_RECORDING_WORKER_STACK_SIZE, (void *)(uintptr_t)job,
                    CONFIG_RECORDING_WORKER_PRIORITY, NULL) == pdPASS) {
        return;
    }
    ESP_LOGE(TAG, "worker could not start");
    /* Nothing was touched by a capture that never started; a pending cleanup stays pending. */
    if (job == JOB_CAPTURE) {
        report_failed(RECORDING_FAILURE_START, true);
    } else {
        report_failed(RECORDING_FAILURE_CLEANUP, false);
    }
}

static void dispatch(recording_effect_t (*command)(recording_state_t *, uint64_t), uint64_t generation)
{
    recording_view_t view;
    taskENTER_CRITICAL(&lock);
    const recording_effect_t effect = command(&state, generation);
    recording_state_view(&state, &view);
    taskEXIT_CRITICAL(&lock);
    if (effect == RECORDING_EFFECT_NONE) {
        return;
    }
    publish(&view);
    if (effect == RECORDING_EFFECT_START_CAPTURE || effect == RECORDING_EFFECT_START_CLEANUP) {
        start_worker(effect == RECORDING_EFFECT_START_CAPTURE ? JOB_CAPTURE : JOB_CLEANUP);
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
    start_worker(JOB_CLEANUP);
    return ESP_OK;
}
