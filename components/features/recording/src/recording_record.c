#include "recording_record.h"

#include <stdint.h>
#include <string.h>

#include "nvs.h"
#include "storage.h"

/* A new namespace marks an incompatible record; the old one is left untouched. */
#define RECORD_NAMESPACE "recording_v1"
#define RECORD_KEY "attempt"

typedef struct {
    uint8_t phase;
    char id[BRIDGE_UUID_SIZE];
} stored_record_t;

static bool phase_valid(uint8_t phase)
{
    return phase <= RECORDING_RECORD_UNKNOWN;
}

static bool stored_valid(const stored_record_t *stored)
{
    if (!phase_valid(stored->phase)) {
        return false;
    }
    if (stored->phase == RECORDING_RECORD_NONE) {
        for (size_t i = 0; i < sizeof(stored->id); ++i) {
            if (stored->id[i] != '\0') {
                return false;
            }
        }
        return true;
    }
    return bridge_uuid_valid(stored->id);
}

esp_err_t recording_record_load(recording_record_t *record)
{
    stored_record_t stored;
    memset(record, 0, sizeof(*record));
    esp_err_t error = storage_get_blob(RECORD_NAMESPACE, RECORD_KEY, &stored, sizeof(stored));
    if (error == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_OK;
    }
    if (error == ESP_OK && !stored_valid(&stored)) {
        error = ESP_ERR_INVALID_STATE;
    }
    if (error == ESP_OK) {
        record->phase = (recording_record_phase_t)stored.phase;
        memcpy(record->id, stored.id, sizeof(record->id));
    }
    return error;
}

static esp_err_t store(recording_record_phase_t phase, const char *id)
{
    stored_record_t stored = {.phase = (uint8_t)phase};
    if (id != NULL) {
        memcpy(stored.id, id, BRIDGE_UUID_SIZE);
    }
    return storage_set_blob(RECORD_NAMESPACE, RECORD_KEY, &stored, sizeof(stored));
}

esp_err_t recording_record_begin(const char *id)
{
    return bridge_uuid_valid(id) ? store(RECORDING_RECORD_PENDING, id) : ESP_ERR_INVALID_ARG;
}

esp_err_t recording_record_finish(const char *id, recording_record_phase_t terminal)
{
    if (!bridge_uuid_valid(id) || terminal < RECORDING_RECORD_COMPLETED || !phase_valid((uint8_t)terminal)) {
        return ESP_ERR_INVALID_ARG;
    }
    return store(terminal, id);
}

esp_err_t recording_record_clear(void)
{
    return store(RECORDING_RECORD_NONE, NULL);
}
