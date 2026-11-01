#include "unity.h"
#include "wifi_select.h"

void setUp(void) {}
void tearDown(void) {}

static void all_unseen(int16_t best[WIFI_LIST_CAPACITY])
{
    for (unsigned i = 0; i < WIFI_LIST_CAPACITY; ++i) {
        best[i] = WIFI_SELECT_UNSEEN;
    }
}

static uint8_t expect_connect(wifi_select_t *state, uint64_t now_ms)
{
    uint8_t index = 0xFF;
    TEST_ASSERT_EQUAL(WIFI_SELECT_CONNECT, wifi_select_step(state, now_ms, &index));
    return index;
}

static void expect_action(wifi_select_t *state, uint64_t now_ms, wifi_select_action_t action)
{
    uint8_t index;
    TEST_ASSERT_EQUAL(action, wifi_select_step(state, now_ms, &index));
}

static void test_boot_scans_then_tries_seen_by_signal_then_unseen_in_saved_order(void)
{
    wifi_list_t list = {0};
    wifi_list_save(&list, "hidden", "password", NULL); /* index 3 after the others */
    wifi_list_save(&list, "weak", "password", NULL);
    wifi_list_save(&list, "strong", "password", NULL);
    wifi_list_save(&list, "missing", "password", NULL); /* index 0 */
    wifi_select_t state;
    wifi_select_reset(&state, 0);
    expect_action(&state, 0, WIFI_SELECT_SCAN);

    int16_t best[WIFI_LIST_CAPACITY];
    all_unseen(best);
    wifi_select_note(&list, "weak", -80, best);
    wifi_select_note(&list, "strong", -60, best);
    wifi_select_note(&list, "weak", -50, best); /* best observation of the same SSID */
    wifi_select_note(&list, "stranger", -10, best);
    wifi_select_round(&state, list.count, best);

    TEST_ASSERT_EQUAL_UINT8(wifi_list_find(&list, "weak"), expect_connect(&state, 0));
    wifi_select_disconnected(&state, false, 100);
    TEST_ASSERT_EQUAL_UINT8(wifi_list_find(&list, "strong"), expect_connect(&state, 100));
    wifi_select_disconnected(&state, false, 200);
    TEST_ASSERT_EQUAL_UINT8(0, expect_connect(&state, 200));
    wifi_select_disconnected(&state, false, 300);
    TEST_ASSERT_EQUAL_UINT8(3, expect_connect(&state, 300));
}

static void test_ties_keep_saved_order(void)
{
    wifi_select_t state;
    wifi_select_reset(&state, 0);
    const int16_t best[WIFI_LIST_CAPACITY] = {-70, -60, -70, WIFI_SELECT_UNSEEN, -60};
    wifi_select_round(&state, WIFI_LIST_CAPACITY, best);
    const uint8_t expected[] = {1, 4, 0, 2, 3};
    for (unsigned i = 0; i < sizeof(expected); ++i) {
        TEST_ASSERT_EQUAL_UINT8(expected[i], expect_connect(&state, i));
        wifi_select_disconnected(&state, false, i);
    }
}

static void test_a_failed_scan_tries_every_network_in_saved_order(void)
{
    wifi_select_t state;
    wifi_select_reset(&state, 0);
    int16_t best[WIFI_LIST_CAPACITY];
    all_unseen(best);
    wifi_select_round(&state, 2, best);
    TEST_ASSERT_EQUAL_UINT8(0, expect_connect(&state, 0));
    wifi_select_disconnected(&state, false, 1);
    TEST_ASSERT_EQUAL_UINT8(1, expect_connect(&state, 1));
}

static void test_an_attempt_times_out_after_15_seconds(void)
{
    wifi_select_t state;
    wifi_select_reset(&state, 0);
    int16_t best[WIFI_LIST_CAPACITY];
    all_unseen(best);
    wifi_select_round(&state, 2, best);
    TEST_ASSERT_EQUAL_UINT8(0, expect_connect(&state, 1000));
    TEST_ASSERT_EQUAL_UINT64(1000 + WIFI_SELECT_ATTEMPT_MS, wifi_select_wake_ms(&state, 1000));
    expect_action(&state, 1000 + WIFI_SELECT_ATTEMPT_MS - 1, WIFI_SELECT_WAIT);
    TEST_ASSERT_EQUAL_UINT8(1, expect_connect(&state, 1000 + WIFI_SELECT_ATTEMPT_MS));
}

static void test_an_exhausted_round_waits_30_seconds_before_scanning(void)
{
    wifi_select_t state;
    wifi_select_reset(&state, 0);
    int16_t best[WIFI_LIST_CAPACITY];
    all_unseen(best);
    wifi_select_round(&state, 1, best);
    expect_connect(&state, 0);
    wifi_select_disconnected(&state, false, 500);
    expect_action(&state, 500, WIFI_SELECT_WAIT);
    TEST_ASSERT_EQUAL_UINT64(500 + WIFI_SELECT_ROUND_WAIT_MS, wifi_select_wake_ms(&state, 500));
    expect_action(&state, 500 + WIFI_SELECT_ROUND_WAIT_MS, WIFI_SELECT_SCAN);
}

static void test_a_stable_link_is_kept_until_it_is_lost(void)
{
    wifi_select_t state;
    wifi_select_reset(&state, 0);
    const int16_t best[WIFI_LIST_CAPACITY] = {-80, -40, WIFI_SELECT_UNSEEN, WIFI_SELECT_UNSEEN, WIFI_SELECT_UNSEEN};
    wifi_select_round(&state, 2, best);
    TEST_ASSERT_EQUAL_UINT8(1, expect_connect(&state, 0));
    wifi_select_connected(&state, 1);
    expect_action(&state, 1000000, WIFI_SELECT_WAIT);
    TEST_ASSERT_EQUAL_UINT64(WIFI_SELECT_NO_WAKE, wifi_select_wake_ms(&state, 1000000));
}

static void test_a_lost_link_retries_every_5_seconds_for_30_then_scans(void)
{
    wifi_select_t state;
    wifi_select_reset(&state, 0);
    wifi_select_connected(&state, 2);
    wifi_select_disconnected(&state, true, 10000);
    TEST_ASSERT_EQUAL_UINT8(2, expect_connect(&state, 10000));
    wifi_select_disconnected(&state, false, 10100);
    expect_action(&state, 10100, WIFI_SELECT_WAIT);
    TEST_ASSERT_EQUAL_UINT64(10000 + WIFI_SELECT_LOSS_RETRY_MS, wifi_select_wake_ms(&state, 10100));
    TEST_ASSERT_EQUAL_UINT8(2, expect_connect(&state, 10000 + WIFI_SELECT_LOSS_RETRY_MS));
    wifi_select_disconnected(&state, false, 15100);
    expect_action(&state, 10000 + WIFI_SELECT_LOSS_WINDOW_MS, WIFI_SELECT_SCAN);
}

static void test_a_preferred_network_is_tried_first_and_its_failure_scans(void)
{
    wifi_select_t state;
    wifi_select_reset(&state, 0);
    wifi_select_prefer(&state, 3, 0);
    TEST_ASSERT_EQUAL_UINT8(3, expect_connect(&state, 0));
    wifi_select_disconnected(&state, false, 2000);
    expect_action(&state, 2000, WIFI_SELECT_SCAN);
}

static void test_setup_pauses_scans_and_rounds_but_not_the_preferred_network(void)
{
    wifi_select_t state;
    wifi_select_reset(&state, 0);
    wifi_select_set_paused(&state, true, 0);
    expect_action(&state, 0, WIFI_SELECT_WAIT);
    TEST_ASSERT_EQUAL_UINT64(WIFI_SELECT_NO_WAKE, wifi_select_wake_ms(&state, 0));

    /* A network saved from the form is tried; its failure waits for setup to close. */
    wifi_select_prefer(&state, 0, 100);
    TEST_ASSERT_EQUAL_UINT8(0, expect_connect(&state, 100));
    wifi_select_disconnected(&state, false, 200);
    expect_action(&state, 200, WIFI_SELECT_WAIT);
    TEST_ASSERT_EQUAL_UINT64(WIFI_SELECT_NO_WAKE, wifi_select_wake_ms(&state, 200));

    wifi_select_set_paused(&state, false, 900000);
    expect_action(&state, 900000, WIFI_SELECT_SCAN);
}

static void test_opening_setup_stops_a_round_in_progress(void)
{
    wifi_select_t state;
    wifi_select_reset(&state, 0);
    int16_t best[WIFI_LIST_CAPACITY];
    all_unseen(best);
    wifi_select_round(&state, 3, best);
    TEST_ASSERT_EQUAL_UINT8(0, expect_connect(&state, 0));
    wifi_select_set_paused(&state, true, 10);
    /* The attempt in progress finishes; the rest of the round does not run. */
    wifi_select_disconnected(&state, false, 20);
    expect_action(&state, 20, WIFI_SELECT_WAIT);
    wifi_select_set_paused(&state, false, 30);
    expect_action(&state, 30, WIFI_SELECT_SCAN);
}

static void test_setup_keeps_the_recovery_of_a_lost_link(void)
{
    wifi_select_t state;
    wifi_select_reset(&state, 0);
    wifi_select_connected(&state, 1);
    wifi_select_set_paused(&state, true, 0);
    expect_action(&state, 0, WIFI_SELECT_WAIT);
    wifi_select_disconnected(&state, true, 1000);
    TEST_ASSERT_EQUAL_UINT8(1, expect_connect(&state, 1000));
    wifi_select_disconnected(&state, false, 1100);
    /* After the recovery window, no full round runs during setup. */
    expect_action(&state, 1000 + WIFI_SELECT_LOSS_WINDOW_MS, WIFI_SELECT_WAIT);
    wifi_select_set_paused(&state, false, 40000);
    expect_action(&state, 40000, WIFI_SELECT_SCAN);
}

static void test_closing_setup_with_a_link_keeps_it(void)
{
    wifi_select_t state;
    wifi_select_reset(&state, 0);
    wifi_select_set_paused(&state, true, 0);
    wifi_select_prefer(&state, 0, 0);
    expect_connect(&state, 0);
    wifi_select_connected(&state, 0);
    wifi_select_set_paused(&state, false, 100);
    expect_action(&state, 100, WIFI_SELECT_WAIT);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_boot_scans_then_tries_seen_by_signal_then_unseen_in_saved_order);
    RUN_TEST(test_ties_keep_saved_order);
    RUN_TEST(test_a_failed_scan_tries_every_network_in_saved_order);
    RUN_TEST(test_an_attempt_times_out_after_15_seconds);
    RUN_TEST(test_an_exhausted_round_waits_30_seconds_before_scanning);
    RUN_TEST(test_a_stable_link_is_kept_until_it_is_lost);
    RUN_TEST(test_a_lost_link_retries_every_5_seconds_for_30_then_scans);
    RUN_TEST(test_a_preferred_network_is_tried_first_and_its_failure_scans);
    RUN_TEST(test_setup_pauses_scans_and_rounds_but_not_the_preferred_network);
    RUN_TEST(test_opening_setup_stops_a_round_in_progress);
    RUN_TEST(test_setup_keeps_the_recovery_of_a_lost_link);
    RUN_TEST(test_closing_setup_with_a_link_keeps_it);
    return UNITY_END();
}
