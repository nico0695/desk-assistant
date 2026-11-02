#include "bridge_uuid.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static void canonical_uuid_is_required(void)
{
    TEST_ASSERT_TRUE(bridge_uuid_valid("12345678-1234-abcd-9876-123456789abc"));
    TEST_ASSERT_FALSE(bridge_uuid_valid(NULL));
    TEST_ASSERT_FALSE(bridge_uuid_valid(""));
    TEST_ASSERT_FALSE(bridge_uuid_valid("12345678-1234-ABCD-9876-123456789abc"));
    TEST_ASSERT_FALSE(bridge_uuid_valid("12345678_1234-abcd-9876-123456789abc"));
    TEST_ASSERT_FALSE(bridge_uuid_valid("12345678-1234-abcd-9876-123456789ab"));
    TEST_ASSERT_FALSE(bridge_uuid_valid("12345678-1234-abcd-9876-123456789abcd"));
    TEST_ASSERT_FALSE(bridge_uuid_valid("12345678-1234-abcd-9876-123456789abg"));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(canonical_uuid_is_required);
    return UNITY_END();
}
