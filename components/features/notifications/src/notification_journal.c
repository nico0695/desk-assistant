#include "notification_journal.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "nvs.h"
#include "storage.h"

#define NOTIFICATION_NAMESPACE "notifications_v1"
#define NOTIFICATION_KEY "journal"
#define NOTIFICATION_MAGIC UINT32_C(0x4e4f5446)
#define NOTIFICATION_VERSION 1U

typedef struct {
    uint32_t magic;
    uint32_t version;
    notification_state_t state;
    uint32_t checksum;
} stored_journal_t;

static uint32_t checksum(const void *data, size_t size)
{
    const unsigned char *bytes = data;
    uint32_t crc = UINT32_MAX;
    for (size_t i = 0; i < size; ++i) {
        crc ^= bytes[i];
        for (unsigned bit = 0; bit < 8U; ++bit) {
            crc = (crc >> 1U) ^ ((crc & 1U) != 0U ? UINT32_C(0xedb88320) : 0U);
        }
    }
    return ~crc;
}

esp_err_t notification_journal_load(notification_state_t *state)
{
    if (state == NULL) return ESP_ERR_INVALID_ARG;
    notification_state_init(state);
    stored_journal_t *stored = malloc(sizeof(*stored));
    if (stored == NULL) return ESP_ERR_NO_MEM;
    esp_err_t error = storage_get_blob(NOTIFICATION_NAMESPACE, NOTIFICATION_KEY, stored, sizeof(*stored));
    if (error == ESP_ERR_NVS_NOT_FOUND) error = ESP_OK;
    else if (error == ESP_OK) {
        if (stored->magic != NOTIFICATION_MAGIC || stored->version != NOTIFICATION_VERSION ||
            checksum(stored, offsetof(stored_journal_t, checksum)) != stored->checksum ||
            !notification_state_valid(&stored->state)) error = ESP_ERR_INVALID_STATE;
        else *state = stored->state;
    }
    free(stored);
    return error;
}

esp_err_t notification_journal_save(const notification_state_t *state)
{
    if (!notification_state_valid(state)) return ESP_ERR_INVALID_ARG;
    stored_journal_t *stored = calloc(1, sizeof(*stored));
    if (stored == NULL) return ESP_ERR_NO_MEM;
    stored->magic = NOTIFICATION_MAGIC;
    stored->version = NOTIFICATION_VERSION;
    stored->state = *state;
    stored->checksum = checksum(stored, offsetof(stored_journal_t, checksum));
    const esp_err_t error = storage_set_blob(NOTIFICATION_NAMESPACE, NOTIFICATION_KEY, stored, sizeof(*stored));
    free(stored);
    return error;
}
