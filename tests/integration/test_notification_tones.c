#include "notification_tones.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static void saturates_at_eight_and_drains_once_per_request(void)
{
    notification_tones_t tones = {0};
    for (unsigned i = 0U; i < 12U; ++i) notification_tones_add(&tones);
    TEST_ASSERT_EQUAL_UINT(NOTIFICATION_TONES_MAX_PENDING, tones.pending);
    for (unsigned i = 0U; i < NOTIFICATION_TONES_MAX_PENDING; ++i)
        TEST_ASSERT_TRUE(notification_tones_take(&tones));
    TEST_ASSERT_FALSE(notification_tones_take(&tones));
    TEST_ASSERT_EQUAL_UINT(0U, tones.pending);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(saturates_at_eight_and_drains_once_per_request);
    return UNITY_END();
}
