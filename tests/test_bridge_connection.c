#include <stdint.h>

#include "bridge_connection.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static void stream_requires_ip_and_origin(void)
{
    bridge_connection_t state;
    bridge_connection_init(&state);
    TEST_ASSERT_FALSE(bridge_connection_is_online(&state));
    TEST_ASSERT_EQUAL_UINT64(0, bridge_connection_begin_stream(&state));
    bridge_connection_set_ip(&state, true);
    TEST_ASSERT_EQUAL_UINT64(0, bridge_connection_begin_stream(&state));
    bridge_connection_origin_changed(&state, true);
    const uint64_t ticket = bridge_connection_begin_stream(&state);
    TEST_ASSERT_NOT_EQUAL(0, ticket);
    TEST_ASSERT_FALSE(bridge_connection_is_online(&state));
    TEST_ASSERT_TRUE(bridge_connection_stream_opened(&state, ticket));
    TEST_ASSERT_TRUE(bridge_connection_is_online(&state));
    bridge_connection_set_ip(&state, false);
    TEST_ASSERT_FALSE(bridge_connection_is_online(&state));
    TEST_ASSERT_FALSE(bridge_connection_stream_opened(&state, ticket));
}

static void origin_and_attempts_invalidate_old_results(void)
{
    bridge_connection_t state;
    bridge_connection_init(&state);
    bridge_connection_origin_changed(&state, true);
    bridge_connection_set_ip(&state, true);
    const uint64_t first = bridge_connection_begin_stream(&state);
    TEST_ASSERT_TRUE(bridge_connection_stream_opened(&state, first));
    bridge_connection_origin_changed(&state, true);
    TEST_ASSERT_FALSE(bridge_connection_is_online(&state));
    TEST_ASSERT_FALSE(bridge_connection_stream_closed(&state, first));
    const uint64_t second = bridge_connection_begin_stream(&state);
    TEST_ASSERT_FALSE(bridge_connection_stream_opened(&state, first));
    TEST_ASSERT_TRUE(bridge_connection_stream_opened(&state, second));
    bridge_connection_set_ip(&state, true);
    TEST_ASSERT_TRUE(bridge_connection_is_current(&state, second));
    TEST_ASSERT_TRUE(bridge_connection_stream_closed(&state, second));
    TEST_ASSERT_FALSE(bridge_connection_stream_opened(&state, second));
    const uint64_t third = bridge_connection_begin_stream(&state);
    TEST_ASSERT_FALSE(bridge_connection_stream_closed(&state, second));
    TEST_ASSERT_TRUE(bridge_connection_stream_opened(&state, third));
    bridge_connection_origin_changed(&state, false);
    TEST_ASSERT_FALSE(bridge_connection_is_online(&state));
    TEST_ASSERT_EQUAL_UINT64(0, bridge_connection_begin_stream(&state));
}

static void generation_exhaustion_does_not_reuse_a_ticket(void)
{
    bridge_connection_t state;
    bridge_connection_init(&state);
    state.has_ip = true;
    state.has_origin = true;
    state.generation = UINT64_MAX - 1U;
    const uint64_t last = bridge_connection_begin_stream(&state);
    TEST_ASSERT_EQUAL_UINT64(UINT64_MAX, last);
    TEST_ASSERT_TRUE(bridge_connection_stream_opened(&state, last));
    TEST_ASSERT_TRUE(bridge_connection_stream_closed(&state, last));
    TEST_ASSERT_FALSE(bridge_connection_is_current(&state, last));
    TEST_ASSERT_FALSE(bridge_connection_is_online(&state));
    TEST_ASSERT_EQUAL_UINT64(0, bridge_connection_begin_stream(&state));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(stream_requires_ip_and_origin);
    RUN_TEST(origin_and_attempts_invalidate_old_results);
    RUN_TEST(generation_exhaustion_does_not_reuse_a_ticket);
    return UNITY_END();
}
