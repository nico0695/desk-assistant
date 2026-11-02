#include <stdio.h>
#include <string.h>

#include "notification_state.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static bridge_event_t event(unsigned number)
{
    bridge_event_t item = {.alert_id = number + 1U};
    snprintf(item.event_id, sizeof(item.event_id), "00000000-0000-4000-8000-%012u", number);
    strcpy(item.message, "Alert");
    strcpy(item.scheduled_at, "2026-11-02T12:00:00Z");
    strcpy(item.triggered_at, "2026-11-02T12:00:01Z");
    return item;
}

static void close_before_ack_keeps_tombstone_and_deduplicates(void)
{
    notification_state_t state;
    notification_state_init(&state);
    bridge_event_t first = event(1);
    TEST_ASSERT_EQUAL_INT(NOTIFICATION_RECEIVED, notification_state_receive(&state, &first));
    TEST_ASSERT_TRUE(notification_state_close(&state, first.event_id));
    TEST_ASSERT_EQUAL_UINT(0, state.count);
    TEST_ASSERT_EQUAL_STRING(first.event_id, state.pending_ack);
    TEST_ASSERT_EQUAL_INT(NOTIFICATION_DUPLICATE, notification_state_receive(&state, &first));
    TEST_ASSERT_TRUE(notification_state_acknowledged(&state, first.event_id));
    TEST_ASSERT_FALSE(notification_state_has_work(&state));
}

static void full_queue_keeps_front_and_accepts_after_close(void)
{
    notification_state_t state;
    notification_state_init(&state);
    for (unsigned i = 0; i < BRIDGE_EVENT_QUEUE_CAPACITY; ++i) {
        bridge_event_t item = event(i);
        TEST_ASSERT_EQUAL_INT(NOTIFICATION_RECEIVED, notification_state_receive(&state, &item));
        TEST_ASSERT_TRUE(notification_state_acknowledged(&state, item.event_id));
    }
    bridge_event_t extra = event(8);
    TEST_ASSERT_EQUAL_INT(NOTIFICATION_FULL, notification_state_receive(&state, &extra));
    TEST_ASSERT_EQUAL_UINT(BRIDGE_EVENT_QUEUE_CAPACITY, state.count);
    TEST_ASSERT_FALSE(notification_state_close(&state, extra.event_id));
    TEST_ASSERT_TRUE(notification_state_close(&state, state.cards[0].event_id));
    TEST_ASSERT_EQUAL_INT(NOTIFICATION_RECEIVED, notification_state_receive(&state, &extra));
    TEST_ASSERT_EQUAL_STRING(extra.event_id, state.cards[BRIDGE_EVENT_QUEUE_CAPACITY - 1U].event_id);
    TEST_ASSERT_TRUE(notification_state_valid(&state));
}

static void duplicate_after_ack_rearms_displayed_without_inserting(void)
{
    notification_state_t state;
    notification_state_init(&state);
    bridge_event_t first = event(1);
    TEST_ASSERT_EQUAL_INT(NOTIFICATION_RECEIVED, notification_state_receive(&state, &first));
    TEST_ASSERT_TRUE(notification_state_acknowledged(&state, first.event_id));
    TEST_ASSERT_EQUAL_INT(NOTIFICATION_DUPLICATE, notification_state_receive(&state, &first));
    TEST_ASSERT_EQUAL_UINT(1, state.count);
    TEST_ASSERT_EQUAL_STRING(first.event_id, state.pending_ack);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(close_before_ack_keeps_tombstone_and_deduplicates);
    RUN_TEST(full_queue_keeps_front_and_accepts_after_close);
    RUN_TEST(duplicate_after_ack_rearms_displayed_without_inserting);
    return UNITY_END();
}
