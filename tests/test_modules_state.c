#include "bridge_snooze.h"
#include "modules_state.h"
#include "unity.h"

#define T0 "2026-11-02T12:00:00Z"

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
    modules_state_detail_done(&state, request.ticket, MODULES_ERROR_NONE, false);
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
    TEST_ASSERT_EQUAL(MODULES_EFFECT_DISCARD, modules_state_detail_done(&state, request.ticket, MODULES_ERROR_NONE, false));
    TEST_ASSERT_EQUAL(MODULES_SCREEN_LIST, view().screen);
    TEST_ASSERT_EQUAL(MODULES_READY, view().status);
}

static void a_detail_error_keeps_back_and_refresh_rereads_the_same_id(void)
{
    const uint64_t ticket = open_ready();
    modules_state_open_detail(&state, ticket, 102U);
    const modules_request_t request = modules_state_claim(&state);
    modules_state_detail_done(&state, request.ticket, MODULES_ERROR_NOT_FOUND, false);
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
    modules_state_detail_done(&state, state.running_request.ticket, MODULES_ERROR_NONE, false);
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

/* Opens a module and a ready detail of its first entry (id 101); for tasks, pending says whether it is pending. */
static uint64_t open_detail_of(bridge_resource_kind_t kind, bool pending)
{
    modules_state_init(&state);
    modules_state_enter(&state, kind);
    modules_request_t request = modules_state_claim(&state);
    const modules_page_t page = page_of(1U, false, true, 5U);
    modules_state_list_done(&state, request.ticket, MODULES_ERROR_NONE, &page);
    modules_state_finish(&state);
    modules_state_open_detail(&state, state.ticket, 101U);
    request = modules_state_claim(&state);
    modules_state_detail_done(&state, request.ticket, MODULES_ERROR_NONE, pending);
    modules_state_finish(&state);
    TEST_ASSERT_EQUAL(MODULES_READY, view().status);
    return state.ticket;
}

static void actions_follow_the_resource_and_its_state(void)
{
    open_detail_of(BRIDGE_RESOURCE_NOTES, false);
    TEST_ASSERT_TRUE(view().can_delete);
    TEST_ASSERT_FALSE(view().can_complete);
    TEST_ASSERT_FALSE(view().can_snooze);
    uint64_t ticket = open_detail_of(BRIDGE_RESOURCE_TASKS, false);
    TEST_ASSERT_FALSE(view().can_complete);
    TEST_ASSERT_EQUAL(MODULES_EFFECT_NONE, modules_state_act(&state, ticket, MODULES_ACTION_COMPLETE, 0U, NULL));
    open_detail_of(BRIDGE_RESOURCE_TASKS, true);
    TEST_ASSERT_TRUE(view().can_complete);
    ticket = open_detail_of(BRIDGE_RESOURCE_ALERTS, false);
    TEST_ASSERT_TRUE(view().can_snooze);
    TEST_ASSERT_EQUAL(MODULES_EFFECT_NONE, modules_state_act(&state, ticket, MODULES_ACTION_SNOOZE, 4U, T0));
    TEST_ASSERT_EQUAL(MODULES_EFFECT_NONE, modules_state_act(&state, ticket, MODULES_ACTION_SNOOZE, 1U, ""));
    TEST_ASSERT_EQUAL(MODULES_EFFECT_NONE, modules_state_act(&state, ticket, MODULES_ACTION_COMPLETE, 0U, NULL));
}

static void an_action_dispatches_once_and_blocks_every_control(void)
{
    const uint64_t ticket = open_detail_of(BRIDGE_RESOURCE_TASKS, true);
    const unsigned effects = modules_state_act(&state, ticket, MODULES_ACTION_COMPLETE, 0U, NULL);
    TEST_ASSERT_BITS(MODULES_EFFECT_WORK | MODULES_EFFECT_FREE_LIST, MODULES_EFFECT_WORK | MODULES_EFFECT_FREE_LIST,
                     effects);
    TEST_ASSERT_EQUAL(MODULES_ACTING, view().status);
    TEST_ASSERT_EQUAL(MODULES_NOTICE_WORKING, view().notice);
    TEST_ASSERT_EQUAL(MODULES_ACTION_COMPLETE, view().action);
    TEST_ASSERT_FALSE(view().can_delete);
    /* A double tap with the same ticket, or any other command, does nothing while it runs. */
    TEST_ASSERT_EQUAL(MODULES_EFFECT_NONE, modules_state_act(&state, ticket, MODULES_ACTION_COMPLETE, 0U, NULL));
    TEST_ASSERT_EQUAL(MODULES_EFFECT_NONE, modules_state_act(&state, state.ticket, MODULES_ACTION_DELETE, 0U, NULL));
    TEST_ASSERT_EQUAL(MODULES_EFFECT_NONE, modules_state_back(&state, state.ticket));
    TEST_ASSERT_EQUAL(MODULES_EFFECT_NONE, modules_state_refresh(&state, state.ticket));
    const modules_request_t request = modules_state_claim(&state);
    TEST_ASSERT_EQUAL(MODULES_REQUEST_ACTION, request.type);
    TEST_ASSERT_EQUAL(MODULES_ACTION_COMPLETE, request.action);
    TEST_ASSERT_EQUAL_UINT64(101U, request.number);
    TEST_ASSERT_EQUAL(MODULES_REQUEST_NONE, modules_state_finish(&state).type);
}

static void a_delete_success_reads_the_origin_page_and_keeps_its_notice(void)
{
    const uint64_t ticket = open_detail_of(BRIDGE_RESOURCE_NOTES, false);
    modules_state_act(&state, ticket, MODULES_ACTION_DELETE, 0U, NULL);
    modules_request_t request = modules_state_claim(&state);
    TEST_ASSERT_BITS(MODULES_EFFECT_WORK | MODULES_EFFECT_FREE_DETAIL, MODULES_EFFECT_WORK | MODULES_EFFECT_FREE_DETAIL,
                     modules_state_action_done(&state, request.ticket, MODULES_NOTICE_DONE, ""));
    TEST_ASSERT_EQUAL(MODULES_SCREEN_LIST, view().screen);
    TEST_ASSERT_EQUAL(MODULES_LOADING, view().status);
    TEST_ASSERT_EQUAL(MODULES_NOTICE_DONE, view().notice);
    request = modules_state_finish(&state);
    TEST_ASSERT_EQUAL(MODULES_REQUEST_LIST, request.type);
    TEST_ASSERT_EQUAL_UINT64(1U, request.number);
    /* The success stays visible when the fresh list fails. */
    modules_state_list_done(&state, request.ticket, MODULES_ERROR_NETWORK, NULL);
    TEST_ASSERT_EQUAL(MODULES_FAILED, view().status);
    TEST_ASSERT_EQUAL(MODULES_NOTICE_DONE, view().notice);
    TEST_ASSERT_EQUAL(MODULES_ACTION_DELETE, view().action);
    /* The next user command ends it. */
    modules_state_refresh(&state, state.ticket);
    TEST_ASSERT_EQUAL(MODULES_NOTICE_NONE, view().notice);
}

static void a_snooze_success_rereads_the_detail_with_the_confirmed_time(void)
{
    const uint64_t ticket = open_detail_of(BRIDGE_RESOURCE_ALERTS, false);
    modules_state_act(&state, ticket, MODULES_ACTION_SNOOZE, BRIDGE_SNOOZE_ONE_HOUR, T0);
    modules_request_t request = modules_state_claim(&state);
    TEST_ASSERT_EQUAL_STRING(T0, request.scheduled_at);
    TEST_ASSERT_EQUAL_UINT(BRIDGE_SNOOZE_ONE_HOUR, request.preset);
    modules_state_action_done(&state, request.ticket, MODULES_NOTICE_DONE, "2026-11-02T13:00:00.000Z");
    TEST_ASSERT_EQUAL(MODULES_SCREEN_DETAIL, view().screen);
    TEST_ASSERT_EQUAL(MODULES_LOADING, view().status);
    TEST_ASSERT_EQUAL_STRING("2026-11-02T13:00:00.000Z", view().snoozed_until);
    request = modules_state_finish(&state);
    TEST_ASSERT_EQUAL(MODULES_REQUEST_DETAIL, request.type);
    modules_state_detail_done(&state, request.ticket, MODULES_ERROR_NONE, false);
    TEST_ASSERT_EQUAL(MODULES_NOTICE_DONE, view().notice);
    TEST_ASSERT_TRUE(view().can_snooze);
}

static void an_uncertain_outcome_requires_a_fresh_detail(void)
{
    const uint64_t ticket = open_detail_of(BRIDGE_RESOURCE_NOTES, false);
    modules_state_act(&state, ticket, MODULES_ACTION_DELETE, 0U, NULL);
    modules_request_t request = modules_state_claim(&state);
    TEST_ASSERT_EQUAL(MODULES_EFFECT_VIEW,
                      modules_state_action_done(&state, request.ticket, MODULES_NOTICE_UNCONFIRMED, NULL));
    modules_state_finish(&state);
    TEST_ASSERT_EQUAL(MODULES_READY, view().status);
    TEST_ASSERT_EQUAL(MODULES_SCREEN_DETAIL, view().screen);
    TEST_ASSERT_FALSE(view().can_delete);
    TEST_ASSERT_EQUAL(MODULES_EFFECT_NONE, modules_state_act(&state, state.ticket, MODULES_ACTION_DELETE, 0U, NULL));
    /* A failed reread keeps the block; a fresh detail lifts it. */
    modules_state_refresh(&state, state.ticket);
    request = modules_state_claim(&state);
    modules_state_detail_done(&state, request.ticket, MODULES_ERROR_NETWORK, false);
    modules_state_finish(&state);
    TEST_ASSERT_FALSE(view().can_delete);
    modules_state_refresh(&state, state.ticket);
    request = modules_state_claim(&state);
    modules_state_detail_done(&state, request.ticket, MODULES_ERROR_NONE, false);
    TEST_ASSERT_TRUE(view().can_delete);
}

static void a_not_sent_outcome_allows_a_new_explicit_action(void)
{
    const uint64_t ticket = open_detail_of(BRIDGE_RESOURCE_NOTES, false);
    modules_state_act(&state, ticket, MODULES_ACTION_DELETE, 0U, NULL);
    const modules_request_t request = modules_state_claim(&state);
    modules_state_action_done(&state, request.ticket, MODULES_NOTICE_BUSY, NULL);
    modules_state_finish(&state);
    TEST_ASSERT_EQUAL(MODULES_NOTICE_BUSY, view().notice);
    TEST_ASSERT_TRUE(view().can_delete);
    /* Back after dispatch reads the list again: the retained copy ended with the action. */
    TEST_ASSERT_BITS(MODULES_EFFECT_WORK, MODULES_EFFECT_WORK, modules_state_back(&state, state.ticket));
}

static void an_action_that_cannot_start_a_worker_is_not_sent(void)
{
    const uint64_t ticket = open_detail_of(BRIDGE_RESOURCE_NOTES, false);
    modules_state_act(&state, ticket, MODULES_ACTION_DELETE, 0U, NULL);
    modules_state_claim(&state);
    modules_state_start_failed(&state);
    TEST_ASSERT_EQUAL(MODULES_NOTICE_NOT_SENT, view().notice);
    TEST_ASSERT_EQUAL(MODULES_READY, view().status);
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
    RUN_TEST(actions_follow_the_resource_and_its_state);
    RUN_TEST(an_action_dispatches_once_and_blocks_every_control);
    RUN_TEST(a_delete_success_reads_the_origin_page_and_keeps_its_notice);
    RUN_TEST(a_snooze_success_rereads_the_detail_with_the_confirmed_time);
    RUN_TEST(an_uncertain_outcome_requires_a_fresh_detail);
    RUN_TEST(a_not_sent_outcome_allows_a_new_explicit_action);
    RUN_TEST(an_action_that_cannot_start_a_worker_is_not_sent);
    return UNITY_END();
}
