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

static void random_bytes_format_a_version_4_uuid(void)
{
    uint8_t bytes[16];
    for (unsigned i = 0; i < sizeof(bytes); ++i) {
        bytes[i] = 0xFF;
    }
    char uuid[BRIDGE_UUID_SIZE];
    bridge_uuid_format(bytes, uuid);
    TEST_ASSERT_EQUAL_STRING("ffffffff-ffff-4fff-bfff-ffffffffffff", uuid);
    for (unsigned i = 0; i < sizeof(bytes); ++i) {
        bytes[i] = (uint8_t)i;
    }
    bridge_uuid_format(bytes, uuid);
    TEST_ASSERT_EQUAL_STRING("00010203-0405-4607-8809-0a0b0c0d0e0f", uuid);
    TEST_ASSERT_TRUE(bridge_uuid_valid(uuid));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(canonical_uuid_is_required);
    RUN_TEST(random_bytes_format_a_version_4_uuid);
    return UNITY_END();
}
