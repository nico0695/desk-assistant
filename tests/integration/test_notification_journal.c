#include <stdio.h>
#include <string.h>

#include "notification_journal.h"
#include "nvs.h"
#include "storage.h"
#include "unity.h"

static unsigned char stored[8192];
static size_t stored_size;
static esp_err_t write_result;

esp_err_t storage_set_blob(const char *name_space, const char *key, const void *data, size_t size)
{
    TEST_ASSERT_EQUAL_STRING("notifications_v1", name_space);
    TEST_ASSERT_EQUAL_STRING("journal", key);
    TEST_ASSERT_TRUE(size <= sizeof(stored));
    memcpy(stored, data, size);
    stored_size = size;
    return write_result;
}

esp_err_t storage_get_blob(const char *name_space, const char *key, void *data, size_t size)
{
    TEST_ASSERT_EQUAL_STRING("notifications_v1", name_space);
    TEST_ASSERT_EQUAL_STRING("journal", key);
    if (stored_size == 0U) return ESP_ERR_NVS_NOT_FOUND;
    if (size != stored_size) return ESP_ERR_INVALID_ARG;
    memcpy(data, stored, size);
    return ESP_OK;
}

void setUp(void) { memset(stored, 0, sizeof(stored)); stored_size = 0; write_result = ESP_OK; }
void tearDown(void) {}

static bridge_event_t event(void)
{
    bridge_event_t item = {.alert_id = 1};
    strcpy(item.event_id, "01234567-89ab-cdef-0123-456789abcdef");
    strcpy(item.message, "Doorbell");
    strcpy(item.scheduled_at, "2026-11-02T12:00:00Z");
    strcpy(item.triggered_at, "2026-11-02T12:00:01Z");
    return item;
}

static void closed_card_retains_ack_across_restart(void)
{
    notification_state_t state;
    TEST_ASSERT_EQUAL_INT(ESP_OK, notification_journal_load(&state));
    bridge_event_t item = event();
    TEST_ASSERT_EQUAL_INT(NOTIFICATION_RECEIVED, notification_state_receive(&state, &item));
    TEST_ASSERT_EQUAL_INT(ESP_OK, notification_journal_save(&state));
    TEST_ASSERT_TRUE(notification_state_close(&state, item.event_id));
    TEST_ASSERT_EQUAL_INT(ESP_OK, notification_journal_save(&state));
    notification_state_t restored;
    TEST_ASSERT_EQUAL_INT(ESP_OK, notification_journal_load(&restored));
    TEST_ASSERT_EQUAL_UINT(0, restored.count);
    TEST_ASSERT_EQUAL_STRING(item.event_id, restored.pending_ack);
}

static void corrupt_or_uncertain_write_is_never_read_as_empty(void)
{
    notification_state_t state;
    notification_state_init(&state);
    bridge_event_t item = event();
    TEST_ASSERT_EQUAL_INT(NOTIFICATION_RECEIVED, notification_state_receive(&state, &item));
    write_result = ESP_FAIL;
    TEST_ASSERT_EQUAL_INT(ESP_FAIL, notification_journal_save(&state));
    notification_state_t restored;
    TEST_ASSERT_EQUAL_INT(ESP_OK, notification_journal_load(&restored));
    TEST_ASSERT_EQUAL_UINT(1, restored.count);
    stored[12] ^= 1U;
    TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_STATE, notification_journal_load(&restored));
    TEST_ASSERT_EQUAL_UINT(0, restored.count);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(closed_card_retains_ack_across_restart);
    RUN_TEST(corrupt_or_uncertain_write_is_never_read_as_empty);
    return UNITY_END();
}
