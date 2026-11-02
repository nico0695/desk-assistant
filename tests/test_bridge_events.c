#include <stdio.h>
#include <string.h>

#include "bridge_events.h"
#include "unity.h"

static const char EVENT[] =
    "{\"events\":[{\"event_id\":\"01234567-89ab-cdef-0123-456789abcdef\",\"type\":\"alert.triggered\","
    "\"alert_id\":42,\"message\":\"Wake up\",\"scheduled_at\":\"2026-11-02T12:30:00Z\","
    "\"triggered_at\":\"2026-11-02T12:30:01.123Z\"}]}";

void setUp(void) {}
void tearDown(void) {}

static void valid_and_empty_responses(void)
{
    bridge_event_t event = {0};
    bool present = false;
    TEST_ASSERT_EQUAL_INT(BRIDGE_EVENT_DECODE_OK, bridge_event_decode(EVENT, strlen(EVENT), 200, &event, &present));
    TEST_ASSERT_TRUE(present);
    TEST_ASSERT_EQUAL_UINT64(42, event.alert_id);
    TEST_ASSERT_EQUAL_STRING("Wake up", event.message);
    TEST_ASSERT_EQUAL_INT(BRIDGE_EVENT_DECODE_OK, bridge_event_decode("{\"events\":[]}", 13, 200, &event, &present));
    TEST_ASSERT_FALSE(present);
    TEST_ASSERT_EQUAL_UINT64(0, event.alert_id);
    TEST_ASSERT_TRUE(bridge_event_ack_succeeded(204, 0));
    TEST_ASSERT_FALSE(bridge_event_ack_succeeded(204, 1));
    TEST_ASSERT_FALSE(bridge_event_ack_succeeded(200, 0));
}

static void invalid_responses_leave_outputs_unchanged(void)
{
    const char *invalid[] = {
        "{\"events\":[{},{}]}",
        "{\"events\":[],\"has_more\":false}",
        "{\"events\":[],\"events\":[]}",
        "{\"events\":[{\"event_id\":\"bad\"}]}",
    };
    bridge_event_t event = {.alert_id = 77};
    bool present = true;
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        TEST_ASSERT_EQUAL_INT(BRIDGE_EVENT_DECODE_INVALID,
                              bridge_event_decode(invalid[i], strlen(invalid[i]), 200, &event, &present));
        TEST_ASSERT_EQUAL_UINT64(77, event.alert_id);
        TEST_ASSERT_TRUE(present);
    }
    TEST_ASSERT_EQUAL_INT(BRIDGE_EVENT_DECODE_INVALID, bridge_event_decode(EVENT, strlen(EVENT), 503, &event, &present));
}

static void numeric_and_message_limits_reject_without_ackable_event(void)
{
    static const char format[] =
        "{\"events\":[{\"event_id\":\"01234567-89ab-cdef-0123-456789abcdef\",\"type\":\"alert.triggered\","
        "\"alert_id\":%s,\"message\":\"%s\",\"scheduled_at\":\"2026-11-02T12:30:00Z\","
        "\"triggered_at\":\"2026-11-02T12:30:01Z\"}]}";
    char json[1200];
    bridge_event_t event = {.alert_id = 77};
    bool present = true;
    const char *ids[] = {"0", "1.2", "9007199254740992"};
    for (size_t i = 0; i < sizeof(ids) / sizeof(ids[0]); ++i) {
        int size = snprintf(json, sizeof(json), format, ids[i], "message");
        TEST_ASSERT_EQUAL_INT(BRIDGE_EVENT_DECODE_INVALID,
                              bridge_event_decode(json, (size_t)size, 200, &event, &present));
    }
    char message[BRIDGE_EVENT_MESSAGE_MAX_BYTES + 2U];
    memset(message, 'x', sizeof(message) - 1U);
    message[sizeof(message) - 1U] = '\0';
    int size = snprintf(json, sizeof(json), format, "1", message);
    TEST_ASSERT_EQUAL_INT(BRIDGE_EVENT_DECODE_TOO_LARGE,
                          bridge_event_decode(json, (size_t)size, 200, &event, &present));
    TEST_ASSERT_EQUAL_UINT64(77, event.alert_id);
    TEST_ASSERT_TRUE(present);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(valid_and_empty_responses);
    RUN_TEST(invalid_responses_leave_outputs_unchanged);
    RUN_TEST(numeric_and_message_limits_reject_without_ackable_event);
    return UNITY_END();
}
