#include <string.h>

#include "bridge_snooze.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static void request_uses_frozen_full_timestamp(void)
{
    char path[BRIDGE_SNOOZE_PATH_SIZE];
    char body[BRIDGE_SNOOZE_REQUEST_SIZE];
    TEST_ASSERT_TRUE(bridge_snooze_path(42, path, sizeof(path)));
    TEST_ASSERT_EQUAL_STRING("/v1/device/alerts/42/snooze", path);
    TEST_ASSERT_TRUE(bridge_snooze_request(BRIDGE_SNOOZE_TOMORROW,
        "2026-11-02T12:00:00.123Z", body, sizeof(body)));
    TEST_ASSERT_EQUAL_STRING("{\"preset\":\"snooze_tomorrow_same_time\",\"expected_scheduled_at\":\"2026-11-02T12:00:00.123Z\"}", body);
    TEST_ASSERT_FALSE(bridge_snooze_request(4, "2026-11-02T12:00:00Z", body, sizeof(body)));
    TEST_ASSERT_FALSE(bridge_snooze_request(BRIDGE_SNOOZE_FIVE_MINUTES,
        "2026-11-02T12:00:00+03:00", body, sizeof(body)));
}

static void success_requires_exact_identity_and_utc_time(void)
{
    char confirmed[BRIDGE_EVENT_TIME_SIZE];
    const char *good = "{\"alert_id\":42,\"scheduled_at\":\"2026-11-02T13:00:00Z\"}";
    TEST_ASSERT_EQUAL_INT(BRIDGE_ACTION_SUCCESS,
        bridge_snooze_decode(200, good, strlen(good), 42, confirmed));
    TEST_ASSERT_EQUAL_STRING("2026-11-02T13:00:00Z", confirmed);
    TEST_ASSERT_EQUAL_INT(BRIDGE_ACTION_UNCONFIRMED,
        bridge_snooze_decode(200, good, strlen(good), 43, confirmed));
    TEST_ASSERT_EQUAL_STRING("", confirmed);
    const char *extra = "{\"alert_id\":42,\"scheduled_at\":\"2026-11-02T13:00:00Z\",\"extra\":true}";
    TEST_ASSERT_EQUAL_INT(BRIDGE_ACTION_UNCONFIRMED,
        bridge_snooze_decode(200, extra, strlen(extra), 42, confirmed));
    TEST_ASSERT_EQUAL_INT(BRIDGE_ACTION_UNCONFIRMED,
        bridge_snooze_decode(200, good, strlen(good) - 1U, 42, confirmed));
}

static void known_errors_are_classified_without_message_text(void)
{
    char confirmed[BRIDGE_EVENT_TIME_SIZE];
    const char *changed = "{\"error\":{\"code\":\"alert_schedule_changed\",\"message\":\"private\"}}";
    TEST_ASSERT_EQUAL_INT(BRIDGE_ACTION_CHANGED,
        bridge_snooze_decode(409, changed, strlen(changed), 42, confirmed));
    TEST_ASSERT_EQUAL_INT(BRIDGE_ACTION_UNCONFIRMED,
        bridge_snooze_decode(500, changed, strlen(changed), 42, confirmed));
    const char *missing = "{\"error\":{\"code\":\"alert_not_found\",\"message\":\"private\"}}";
    TEST_ASSERT_EQUAL_INT(BRIDGE_ACTION_NOT_FOUND,
        bridge_snooze_decode(404, missing, strlen(missing), 42, confirmed));
    const char *not_sent = "{\"error\":{\"code\":\"temporarily_unavailable\",\"message\":\"private\"}}";
    TEST_ASSERT_EQUAL_INT(BRIDGE_ACTION_NOT_SENT,
        bridge_snooze_decode(503, not_sent, strlen(not_sent), 42, confirmed));
    /* Rows shared with Delete and Complete: a missing route is a rejection, a read failure stays uncertain. */
    const char *no_route = "{\"error\":{\"code\":\"not_found\",\"message\":\"private\"}}";
    TEST_ASSERT_EQUAL_INT(BRIDGE_ACTION_REJECTED,
        bridge_snooze_decode(404, no_route, strlen(no_route), 42, confirmed));
    const char *read_failed = "{\"error\":{\"code\":\"upstream_unavailable\",\"message\":\"private\"}}";
    TEST_ASSERT_EQUAL_INT(BRIDGE_ACTION_UNCONFIRMED,
        bridge_snooze_decode(502, read_failed, strlen(read_failed), 42, confirmed));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(request_uses_frozen_full_timestamp);
    RUN_TEST(success_requires_exact_identity_and_utc_time);
    RUN_TEST(known_errors_are_classified_without_message_text);
    return UNITY_END();
}
