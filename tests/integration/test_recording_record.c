#include <string.h>

#include "nvs.h"
#include "recording_record.h"
#include "storage.h"
#include "unity.h"

#define ID "12345678-1234-4abc-9876-123456789abc"

static unsigned char stored[64];
static size_t stored_size;
static esp_err_t read_error, write_error;
static unsigned writes;

esp_err_t storage_set_blob(const char *name_space, const char *key, const void *data, size_t size)
{
    TEST_ASSERT_EQUAL_STRING("recording_v1", name_space);
    TEST_ASSERT_EQUAL_STRING("attempt", key);
    ++writes;
    /* A failed commit may still have written. */
    memcpy(stored, data, size);
    stored_size = size;
    return write_error;
}

esp_err_t storage_get_blob(const char *name_space, const char *key, void *data, size_t size)
{
    (void)name_space; (void)key;
    if (read_error != ESP_OK) return read_error;
    if (stored_size == 0U) return ESP_ERR_NVS_NOT_FOUND;
    memcpy(data, stored, size);
    return ESP_OK;
}

void setUp(void) { memset(stored, 0, sizeof(stored)); stored_size = 0; read_error = write_error = ESP_OK; writes = 0; }
void tearDown(void) {}

static void lifecycle_round_trips(void)
{
    recording_record_t record;
    TEST_ASSERT_EQUAL_INT(ESP_OK, recording_record_load(&record));
    TEST_ASSERT_EQUAL_INT(RECORDING_RECORD_NONE, record.phase);
    TEST_ASSERT_EQUAL_INT(ESP_OK, recording_record_begin(ID));
    TEST_ASSERT_EQUAL_INT(ESP_OK, recording_record_load(&record));
    TEST_ASSERT_EQUAL_INT(RECORDING_RECORD_PENDING, record.phase);
    TEST_ASSERT_EQUAL_STRING(ID, record.id);
    TEST_ASSERT_EQUAL_INT(ESP_OK, recording_record_finish(ID, RECORDING_RECORD_UNKNOWN));
    TEST_ASSERT_EQUAL_INT(ESP_OK, recording_record_load(&record));
    TEST_ASSERT_EQUAL_INT(RECORDING_RECORD_UNKNOWN, record.phase);
    TEST_ASSERT_EQUAL_INT(ESP_OK, recording_record_clear());
    TEST_ASSERT_EQUAL_INT(ESP_OK, recording_record_load(&record));
    TEST_ASSERT_EQUAL_INT(RECORDING_RECORD_NONE, record.phase);
    TEST_ASSERT_EQUAL_STRING("", record.id);
}

static void invalid_arguments_write_nothing(void)
{
    TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_ARG, recording_record_begin("not-a-uuid"));
    TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_ARG, recording_record_finish(ID, RECORDING_RECORD_PENDING));
    TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_ARG, recording_record_finish(ID, (recording_record_phase_t)9));
    TEST_ASSERT_EQUAL_UINT(0, writes);
}

static void unreadable_or_invalid_records_are_errors_not_empty(void)
{
    recording_record_t record;
    read_error = ESP_FAIL;
    TEST_ASSERT_EQUAL_INT(ESP_FAIL, recording_record_load(&record));
    TEST_ASSERT_EQUAL_INT(RECORDING_RECORD_NONE, record.phase);
    read_error = ESP_OK;
    TEST_ASSERT_EQUAL_INT(ESP_OK, recording_record_begin(ID));
    stored[0] = 7;
    TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_STATE, recording_record_load(&record));
    stored[0] = RECORDING_RECORD_PENDING;
    stored[1] = 'X';
    TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_STATE, recording_record_load(&record));
    TEST_ASSERT_EQUAL_INT(ESP_OK, recording_record_clear());
    stored[5] = 'a';
    TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_STATE, recording_record_load(&record));
}

static void failed_write_is_reported(void)
{
    write_error = ESP_FAIL;
    TEST_ASSERT_EQUAL_INT(ESP_FAIL, recording_record_begin(ID));
    TEST_ASSERT_EQUAL_UINT(1, writes);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(lifecycle_round_trips);
    RUN_TEST(invalid_arguments_write_nothing);
    RUN_TEST(unreadable_or_invalid_records_are_errors_not_empty);
    RUN_TEST(failed_write_is_reported);
    return UNITY_END();
}
