#include "modules_state.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static modules_state_t state;
static modules_view_t view(void)
{
    modules_view_t result;
    modules_state_view(&state, &result);
    return result;
}

static modules_page_t page_of(uint64_t number, bool previous, bool next, uint8_t count)
{
    modules_page_t page = {.page = number, .has_previous = previous, .has_next = next, .count = count};
    for (uint8_t i = 0; i < count; ++i) page.ids[i] = 100U * number + i + 1U;
    return page;
}

/* Opens the module and delivers page 1 with five entries and a next page. */
static uint64_t open_ready(void)
{
    modules_state_init(&state);
    TEST_ASSERT_BITS(MODULES_EFFECT_WORK, MODULES_EFFECT_WORK, modules_state_enter(&state, BRIDGE_RESOURCE_NOTES));
    const modules_request_t request = modules_state_claim(&state);
    TEST_ASSERT_EQUAL(MODULES_REQUEST_LIST, request.type);
    TEST_ASSERT_EQUAL_UINT64(1U, request.number);
    const modules_page_t page = page_of(1U, false, true, 5U);
    TEST_ASSERT_BITS(MODULES_EFFECT_ACCEPT, MODULES_EFFECT_ACCEPT,
                     modules_state_list_done(&state, request.ticket, MODULES_ERROR_NONE, &page));
    TEST_ASSERT_EQUAL(MODULES_REQUEST_NONE, modules_state_finish(&state).type);
    TEST_ASSERT_EQUAL(MODULES_READY, view().status);
    return state.ticket;
}

static void entering_asks_for_the_first_page(void)
{
    modules_state_init(&state);
    modules_state_enter(&state, BRIDGE_RESOURCE_TASKS);
    TEST_ASSERT_TRUE(view().open);
    TEST_ASSERT_EQUAL(BRIDGE_RESOURCE_TASKS, view().kind);
    TEST_ASSERT_EQUAL(MODULES_SCREEN_LIST, view().screen);
    TEST_ASSERT_EQUAL(MODULES_LOADING, view().status);
    TEST_ASSERT_EQUAL_UINT64(1U, view().page);
    TEST_ASSERT_EQUAL(MODULES_EFFECT_NONE, modules_state_enter(&state, BRIDGE_RESOURCE_KIND_COUNT));
}

static void an_empty_page_is_a_valid_ready_state(void)
{
    modules_state_init(&state);
    modules_state_enter(&state, BRIDGE_RESOURCE_ALERTS);
    const modules_request_t request = modules_state_claim(&state);
    const modules_page_t page = page_of(1U, false, false, 0U);
    modules_state_list_done(&state, request.ticket, MODULES_ERROR_NONE, &page);
    TEST_ASSERT_EQUAL(MODULES_READY, view().status);
    TEST_ASSERT_EQUAL_UINT8(0U, view().count);
    TEST_ASSERT_EQUAL(MODULES_ERROR_NONE, view().error);
    /* Refresh rereads the same empty page: it never jumps to another. */
    TEST_ASSERT_BITS(MODULES_EFFECT_WORK, MODULES_EFFECT_WORK, modules_state_refresh(&state, state.ticket));
    TEST_ASSERT_EQUAL_UINT64(1U, state.wanted.number);
}

static void paging_moves_one_page_and_drops_the_old_list(void)
{
    const uint64_t ticket = open_ready();
    TEST_ASSERT_EQUAL(MODULES_EFFECT_NONE, modules_state_previous(&state, ticket));
    const unsigned effects = modules_state_next(&state, ticket);
    TEST_ASSERT_BITS(MODULES_EFFECT_FREE_LIST, MODULES_EFFECT_FREE_LIST, effects);
    TEST_ASSERT_EQUAL(MODULES_LOADING, view().status);
    TEST_ASSERT_EQUAL_UINT64(2U, view().page);
    TEST_ASSERT_FALSE(view().has_next);
    TEST_ASSERT_EQUAL_UINT8(0U, view().count);
    const modules_request_t request = modules_state_claim(&state);
    TEST_ASSERT_EQUAL_UINT64(2U, request.number);
    const modules_page_t page = page_of(2U, true, false, 3U);
    modules_state_list_done(&state, request.ticket, MODULES_ERROR_NONE, &page);
    TEST_ASSERT_TRUE(view().has_previous);
    TEST_ASSERT_EQUAL(MODULES_EFFECT_NONE, modules_state_next(&state, state.ticket));
    modules_state_previous(&state, state.ticket);
    TEST_ASSERT_EQUAL_UINT64(1U, view().page);
}

static void a_second_tap_with_the_old_ticket_changes_nothing(void)
{
    const uint64_t ticket = open_ready();
    modules_state_next(&state, ticket);
    TEST_ASSERT_EQUAL(MODULES_EFFECT_NONE, modules_state_next(&state, ticket));
    TEST_ASSERT_EQUAL(MODULES_EFFECT_NONE, modules_state_refresh(&state, ticket));
    TEST_ASSERT_EQUAL_UINT64(2U, view().page);
    /* While loading, even a current ticket cannot refresh. */
    TEST_ASSERT_EQUAL(MODULES_EFFECT_NONE, modules_state_refresh(&state, state.ticket));
}

static void a_result_for_an_old_view_is_discarded(void)
{
    const uint64_t ticket = open_ready();
    modules_state_refresh(&state, ticket);
    const modules_request_t first = modules_state_claim(&state);
    /* The worker is still reading when the user leaves and enters another module. */
    modules_state_leave(&state);
    modules_state_enter(&state, BRIDGE_RESOURCE_TASKS);
    TEST_ASSERT_EQUAL(MODULES_REQUEST_NONE, modules_state_claim(&state).type);
    const modules_page_t stale = page_of(1U, false, false, 2U);
    const unsigned effects = modules_state_list_done(&state, first.ticket, MODULES_ERROR_NONE, &stale);
    TEST_ASSERT_EQUAL(MODULES_EFFECT_DISCARD, effects);
    TEST_ASSERT_EQUAL(MODULES_LOADING, view().status);
    TEST_ASSERT_EQUAL(BRIDGE_RESOURCE_TASKS, view().kind);
    /* The wanted request for the new module starts only when the worker finishes. */
    const modules_request_t next = modules_state_finish(&state);
    TEST_ASSERT_EQUAL(MODULES_REQUEST_LIST, next.type);
    TEST_ASSERT_EQUAL(BRIDGE_RESOURCE_TASKS, next.kind);
    TEST_ASSERT_EQUAL_UINT64(state.ticket, next.ticket);
}

static void a_result_for_another_page_is_not_published(void)
{
    modules_state_init(&state);
    modules_state_enter(&state, BRIDGE_RESOURCE_NOTES);
    const modules_request_t request = modules_state_claim(&state);
    const modules_page_t other = page_of(2U, true, false, 1U);
    const unsigned effects = modules_state_list_done(&state, request.ticket, MODULES_ERROR_NONE, &other);
    TEST_ASSERT_BITS(MODULES_EFFECT_DISCARD, MODULES_EFFECT_DISCARD, effects);
    TEST_ASSERT_EQUAL(MODULES_FAILED, view().status);
    TEST_ASSERT_EQUAL(MODULES_ERROR_INVALID, view().error);
}

static void a_read_error_is_not_an_empty_list(void)
{
    modules_state_init(&state);
    modules_state_enter(&state, BRIDGE_RESOURCE_NOTES);
    const modules_request_t request = modules_state_claim(&state);
    modules_state_list_done(&state, request.ticket, MODULES_ERROR_NETWORK, NULL);
    TEST_ASSERT_EQUAL(MODULES_FAILED, view().status);
    TEST_ASSERT_EQUAL(MODULES_ERROR_NETWORK, view().error);
    TEST_ASSERT_FALSE(view().has_previous);
    TEST_ASSERT_FALSE(view().has_next);
    TEST_ASSERT_EQUAL(MODULES_EFFECT_NONE, modules_state_next(&state, state.ticket));
    /* Refresh retries the same page. */
    modules_state_refresh(&state, state.ticket);
    TEST_ASSERT_EQUAL(MODULES_LOADING, view().status);
    TEST_ASSERT_EQUAL_UINT64(1U, state.wanted.number);
}

static void a_detail_retains_the_list_and_back_uses_it_once(void)
{
    const uint64_t ticket = open_ready();
    TEST_ASSERT_EQUAL(MODULES_EFFECT_NONE, modules_state_open_detail(&state, ticket, 999U));
    TEST_ASSERT_EQUAL(MODULES_EFFECT_NONE, modules_state_open_detail(&state, ticket + 1U, 101U));
    const unsigned opened = modules_state_open_detail(&state, ticket, 103U);
    TEST_ASSERT_BITS(MODULES_EFFECT_WORK, MODULES_EFFECT_WORK, opened);
    TEST_ASSERT_EQUAL(0U, opened & MODULES_EFFECT_FREE_LIST);
    TEST_ASSERT_EQUAL(MODULES_SCREEN_DETAIL, view().screen);
    TEST_ASSERT_EQUAL(MODULES_LOADING, view().status);
    const modules_request_t request = modules_state_claim(&state);
    TEST_ASSERT_EQUAL(MODULES_REQUEST_DETAIL, request.type);
    TEST_ASSERT_EQUAL_UINT64(103U, request.number);
    modules_state_detail_done(&state, request.ticket, MODULES_ERROR_NONE);
    modules_state_finish(&state);
    TEST_ASSERT_EQUAL(MODULES_READY, view().status);

    const unsigned back = modules_state_back(&state, state.ticket);
    TEST_ASSERT_BITS(MODULES_EFFECT_FREE_DETAIL, MODULES_EFFECT_FREE_DETAIL, back);
    TEST_ASSERT_EQUAL(0U, back & MODULES_EFFECT_WORK);
    TEST_ASSERT_EQUAL(MODULES_SCREEN_LIST, view().screen);
    TEST_ASSERT_EQUAL(MODULES_READY, view().status);
    TEST_ASSERT_EQUAL_UINT8(5U, view().count);
    TEST_ASSERT_TRUE(view().has_next);
    TEST_ASSERT_FALSE(state.retained);
    TEST_ASSERT_EQUAL(MODULES_REQUEST_NONE, modules_state_claim(&state).type);
}

static void back_without_a_retained_list_reads_it_again(void)
{
    const uint64_t ticket = open_ready();
    modules_state_open_detail(&state, ticket, 101U);
    state.retained = false;
    const unsigned back = modules_state_back(&state, state.ticket);
    TEST_ASSERT_BITS(MODULES_EFFECT_WORK, MODULES_EFFECT_WORK, back);
    TEST_ASSERT_EQUAL(MODULES_LOADING, view().status);
    TEST_ASSERT_EQUAL_UINT64(1U, state.wanted.number);
    TEST_ASSERT_EQUAL(MODULES_REQUEST_LIST, state.wanted.type);
}

static void back_while_the_detail_loads_drops_its_result(void)
{
    const uint64_t ticket = open_ready();
    modules_state_open_detail(&state, ticket, 101U);
    const modules_request_t request = modules_state_claim(&state);
    modules_state_back(&state, state.ticket);
    TEST_ASSERT_EQUAL(MODULES_EFFECT_DISCARD, modules_state_detail_done(&state, request.ticket, MODULES_ERROR_NONE));
    TEST_ASSERT_EQUAL(MODULES_SCREEN_LIST, view().screen);
    TEST_ASSERT_EQUAL(MODULES_READY, view().status);
}

static void a_detail_error_keeps_back_and_refresh_rereads_the_same_id(void)
{
    const uint64_t ticket = open_ready();
    modules_state_open_detail(&state, ticket, 102U);
    const modules_request_t request = modules_state_claim(&state);
    modules_state_detail_done(&state, request.ticket, MODULES_ERROR_NOT_FOUND);
    modules_state_finish(&state);
    TEST_ASSERT_EQUAL(MODULES_FAILED, view().status);
    TEST_ASSERT_EQUAL(MODULES_ERROR_NOT_FOUND, view().error);
    const unsigned effects = modules_state_refresh(&state, state.ticket);
    TEST_ASSERT_BITS(MODULES_EFFECT_FREE_DETAIL, MODULES_EFFECT_FREE_DETAIL, effects);
    TEST_ASSERT_EQUAL(MODULES_REQUEST_DETAIL, state.wanted.type);
    TEST_ASSERT_EQUAL_UINT64(102U, state.wanted.number);
    modules_state_claim(&state);
    modules_state_finish(&state);
    /* Back still returns to the retained list. */
    modules_state_detail_done(&state, state.running_request.ticket, MODULES_ERROR_NONE);
    modules_state_back(&state, state.ticket);
    TEST_ASSERT_EQUAL(MODULES_READY, view().status);
    TEST_ASSERT_EQUAL_UINT8(5U, view().count);
}

static void only_the_newest_wanted_request_survives_a_running_read(void)
{
    modules_state_init(&state);
    modules_state_enter(&state, BRIDGE_RESOURCE_NOTES);
    const modules_request_t first = modules_state_claim(&state);
    modules_state_leave(&state);
    modules_state_enter(&state, BRIDGE_RESOURCE_TASKS);
    modules_state_leave(&state);
    modules_state_enter(&state, BRIDGE_RESOURCE_ALERTS);
    TEST_ASSERT_EQUAL(MODULES_REQUEST_NONE, modules_state_claim(&state).type);
    TEST_ASSERT_EQUAL(MODULES_EFFECT_DISCARD, modules_state_list_done(&state, first.ticket, MODULES_ERROR_NONE, NULL));
    const modules_request_t next = modules_state_finish(&state);
    TEST_ASSERT_EQUAL(BRIDGE_RESOURCE_ALERTS, next.kind);
    TEST_ASSERT_EQUAL(MODULES_REQUEST_NONE, modules_state_finish(&state).type);
    TEST_ASSERT_FALSE(state.running);
}

static void leaving_frees_everything_and_ignores_later_commands(void)
{
    const uint64_t ticket = open_ready();
    const unsigned effects = modules_state_leave(&state);
    TEST_ASSERT_BITS(MODULES_EFFECT_FREE_LIST | MODULES_EFFECT_FREE_DETAIL,
                     MODULES_EFFECT_FREE_LIST | MODULES_EFFECT_FREE_DETAIL, effects);
    TEST_ASSERT_FALSE(view().open);
    TEST_ASSERT_EQUAL(MODULES_EFFECT_NONE, modules_state_next(&state, ticket));
    TEST_ASSERT_EQUAL(MODULES_EFFECT_NONE, modules_state_refresh(&state, state.ticket));
    TEST_ASSERT_EQUAL(MODULES_EFFECT_DISCARD, modules_state_list_done(&state, ticket, MODULES_ERROR_NONE, NULL));
    TEST_ASSERT_EQUAL(MODULES_EFFECT_NONE, modules_state_leave(&state));
}

static void a_worker_that_cannot_start_fails_the_view(void)
{
    modules_state_init(&state);
    modules_state_enter(&state, BRIDGE_RESOURCE_NOTES);
    modules_state_claim(&state);
    const unsigned effects = modules_state_start_failed(&state);
    TEST_ASSERT_BITS(MODULES_EFFECT_VIEW, MODULES_EFFECT_VIEW, effects);
    TEST_ASSERT_EQUAL(MODULES_ERROR_NO_MEMORY, view().error);
    TEST_ASSERT_FALSE(state.running);
    modules_state_refresh(&state, state.ticket);
    TEST_ASSERT_EQUAL(MODULES_REQUEST_LIST, modules_state_claim(&state).type);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(entering_asks_for_the_first_page);
    RUN_TEST(an_empty_page_is_a_valid_ready_state);
    RUN_TEST(paging_moves_one_page_and_drops_the_old_list);
    RUN_TEST(a_second_tap_with_the_old_ticket_changes_nothing);
    RUN_TEST(a_result_for_an_old_view_is_discarded);
    RUN_TEST(a_result_for_another_page_is_not_published);
    RUN_TEST(a_read_error_is_not_an_empty_list);
    RUN_TEST(a_detail_retains_the_list_and_back_uses_it_once);
    RUN_TEST(back_without_a_retained_list_reads_it_again);
    RUN_TEST(back_while_the_detail_loads_drops_its_result);
    RUN_TEST(a_detail_error_keeps_back_and_refresh_rereads_the_same_id);
    RUN_TEST(only_the_newest_wanted_request_survives_a_running_read);
    RUN_TEST(leaving_frees_everything_and_ignores_later_commands);
    RUN_TEST(a_worker_that_cannot_start_fails_the_view);
    return UNITY_END();
}
