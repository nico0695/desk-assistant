#include "notification_sound.h"

#include "board_tone.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "notification_tones.h"
#include "sdkconfig.h"
#include "settings.h"

#define SOUND_WORKER_NAME "notification_sound"

static const char *TAG = "notification_sound";
static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
static notification_tones_t tones;
static TaskHandle_t worker;
static bool starting;

static void run(void *unused)
{
    (void)unused;
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    for (;;) {
        taskENTER_CRITICAL(&lock);
        const bool pending = tones.pending != 0U;
        if (!pending) worker = NULL;
        taskEXIT_CRITICAL(&lock);
        if (!pending) break;
        if (!board_tone_try_claim()) {
            vTaskDelay(pdMS_TO_TICKS(CONFIG_NOTIFICATIONS_SOUND_WAIT_MS));
            continue;
        }
        /* The volume is read once the tone owns the audio, so a wait for a capture does not poll NVS. */
        settings_loaded_t settings;
        settings_load(&settings);
        taskENTER_CRITICAL(&lock);
        (void)notification_tones_take(&tones);
        taskEXIT_CRITICAL(&lock);
        const esp_err_t error = board_tone_play(settings.values[SETTINGS_VOLUME]);
        if (error != ESP_OK) ESP_LOGW(TAG, "tone failed: %s", esp_err_to_name(error));
        esp_err_t cleanup = board_tone_cleanup();
        if (cleanup != ESP_OK) ESP_LOGW(TAG, "tone cleanup pending: %s", esp_err_to_name(cleanup));
        while (cleanup != ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(CONFIG_NOTIFICATIONS_SOUND_CLEANUP_RETRY_MS));
            cleanup = board_tone_cleanup();
        }
    }
    vTaskDelete(NULL);
}

void notification_sound_enqueue(void)
{
    bool create = false;
    taskENTER_CRITICAL(&lock);
    notification_tones_add(&tones);
    /* Notified under the lock: the worker clears its handle under the same lock before it deletes itself. */
    if (worker != NULL) xTaskNotifyGive(worker);
    else if (!starting) { starting = true; create = true; }
    taskEXIT_CRITICAL(&lock);
    if (!create) return;
    TaskHandle_t created = NULL;
    if (xTaskCreate(run, SOUND_WORKER_NAME, CONFIG_NOTIFICATIONS_SOUND_WORKER_STACK_SIZE, NULL,
                    CONFIG_NOTIFICATIONS_SOUND_WORKER_PRIORITY, &created) != pdPASS) {
        taskENTER_CRITICAL(&lock);
        starting = false;
        tones.pending = 0U;
        taskEXIT_CRITICAL(&lock);
        ESP_LOGW(TAG, "tone worker unavailable");
        return;
    }
    taskENTER_CRITICAL(&lock);
    worker = created;
    starting = false;
    taskEXIT_CRITICAL(&lock);
    xTaskNotifyGive(created);
}
