#include "ui_bridge_status.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static void newer_status_wins_even_if_callbacks_arrive_out_of_order(void)
{
    ui_bridge_status_t status = {0};
    TEST_ASSERT_TRUE(ui_bridge_status_update(&status, true, 4));
    TEST_ASSERT_TRUE(ui_bridge_status_update(&status, false, 5));
    TEST_ASSERT_FALSE(ui_bridge_status_update(&status, true, 4));
    TEST_ASSERT_FALSE(status.online);
    TEST_ASSERT_TRUE(ui_bridge_status_update(&status, true, 6));
    TEST_ASSERT_FALSE(ui_bridge_status_update(&status, false, 5));
    TEST_ASSERT_TRUE(status.online);
}

static void disconnected_wins_an_equal_generation(void)
{
    ui_bridge_status_t status = {0};
    TEST_ASSERT_TRUE(ui_bridge_status_update(&status, true, UINT64_MAX));
    TEST_ASSERT_TRUE(ui_bridge_status_update(&status, false, UINT64_MAX));
    TEST_ASSERT_FALSE(ui_bridge_status_update(&status, true, UINT64_MAX));
    TEST_ASSERT_FALSE(status.online);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(newer_status_wins_even_if_callbacks_arrive_out_of_order);
    RUN_TEST(disconnected_wins_an_equal_generation);
    return UNITY_END();
}
