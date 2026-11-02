#include <stdio.h>
#include <string.h>

#include "bridge_resources.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static char arena[BRIDGE_RESOURCE_ARENA_SIZE];
static bridge_resource_page_t page;
static bridge_resource_detail_t detail;

#define T0 "2026-11-02T12:00:00Z"
#define T1 "2026-11-03T09:00:00.500Z"

static bridge_resource_result_t decode_page(bridge_resource_kind_t kind, uint64_t number, int status, const char *body)
{
    return bridge_resource_decode_page(kind, number, status, body, strlen(body), arena, sizeof(arena), &page);
}

static bridge_resource_result_t decode_detail(bridge_resource_kind_t kind, uint64_t id, int status, const char *body)
{
    return bridge_resource_decode_detail(kind, id, status, body, strlen(body), arena, sizeof(arena), &detail);
}

static void paths_are_canonical_and_bounded(void)
{
    char path[BRIDGE_RESOURCE_PATH_SIZE];
    TEST_ASSERT_TRUE(bridge_resource_list_path(BRIDGE_RESOURCE_TASKS, 1U, path, sizeof(path)));
    TEST_ASSERT_EQUAL_STRING("/v1/device/tasks?page=1", path);
    TEST_ASSERT_TRUE(bridge_resource_list_path(BRIDGE_RESOURCE_ALERTS, 12U, path, sizeof(path)));
    TEST_ASSERT_EQUAL_STRING("/v1/device/alerts?page=12", path);
    TEST_ASSERT_TRUE(bridge_resource_detail_path(BRIDGE_RESOURCE_NOTES, 9007199254740991ULL, path, sizeof(path)));
    TEST_ASSERT_EQUAL_STRING("/v1/device/notes/9007199254740991", path);
    TEST_ASSERT_FALSE(bridge_resource_list_path(BRIDGE_RESOURCE_TASKS, 0U, path, sizeof(path)));
    /* The offset (page - 1) * 5 must stay a safe integer. */
    TEST_ASSERT_FALSE(bridge_resource_list_path(BRIDGE_RESOURCE_TASKS, 9007199254740991ULL, path, sizeof(path)));
    TEST_ASSERT_TRUE(bridge_resource_list_path(BRIDGE_RESOURCE_TASKS, 1801439850948199ULL, path, sizeof(path)));
    TEST_ASSERT_FALSE(bridge_resource_list_path(BRIDGE_RESOURCE_TASKS, 1801439850948200ULL, path, sizeof(path)));
    TEST_ASSERT_FALSE(bridge_resource_detail_path(BRIDGE_RESOURCE_NOTES, 0U, path, sizeof(path)));
    TEST_ASSERT_FALSE(bridge_resource_detail_path(BRIDGE_RESOURCE_NOTES, 9007199254740992ULL, path, sizeof(path)));
    TEST_ASSERT_FALSE(bridge_resource_detail_path(BRIDGE_RESOURCE_KIND_COUNT, 1U, path, sizeof(path)));
    TEST_ASSERT_FALSE(bridge_resource_detail_path(BRIDGE_RESOURCE_NOTES, 1U, path, 3U));
}

static void note_page_keeps_order_and_texts(void)
{
    const char *body = "{\"items\":[{\"id\":9,\"title\":\"Segunda \\u00f1\",\"created_at\":\"" T1 "\"},"
                       "{\"id\":4,\"title\":\"First\",\"created_at\":\"" T0 "\"}],\"page\":2,\"page_size\":5,"
                       "\"has_previous\":true,\"has_next\":false}";
    TEST_ASSERT_EQUAL(BRIDGE_RESOURCE_OK, decode_page(BRIDGE_RESOURCE_NOTES, 2U, 200, body));
    TEST_ASSERT_EQUAL_UINT64(2U, page.page);
    TEST_ASSERT_TRUE(page.has_previous);
    TEST_ASSERT_FALSE(page.has_next);
    TEST_ASSERT_EQUAL_UINT8(2U, page.count);
    TEST_ASSERT_EQUAL_UINT64(9U, page.entries[0].id);
    TEST_ASSERT_EQUAL_STRING("Segunda \xc3\xb1", page.entries[0].text);
    TEST_ASSERT_EQUAL_STRING(T1, page.entries[0].created_at);
    TEST_ASSERT_EQUAL_STRING("", page.entries[0].when_at);
    TEST_ASSERT_EQUAL_UINT64(4U, page.entries[1].id);
    TEST_ASSERT_EQUAL_STRING("First", page.entries[1].text);
}

static void task_page_reads_alert_time_or_null(void)
{
    const char *body = "{\"items\":[{\"id\":1,\"title\":\"A\",\"status\":\"pending\",\"alert_at\":\"" T1 "\","
                       "\"created_at\":\"" T0 "\"},{\"id\":2,\"title\":\"B\",\"status\":\"pending\","
                       "\"alert_at\":null,\"created_at\":\"" T0 "\"}],\"page\":1,\"page_size\":5,"
                       "\"has_previous\":false,\"has_next\":false}";
    TEST_ASSERT_EQUAL(BRIDGE_RESOURCE_OK, decode_page(BRIDGE_RESOURCE_TASKS, 1U, 200, body));
    TEST_ASSERT_EQUAL_STRING(T1, page.entries[0].when_at);
    TEST_ASSERT_EQUAL_STRING("", page.entries[1].when_at);
    TEST_ASSERT_EQUAL_STRING(T0, page.entries[1].created_at);
    const char *done = "{\"items\":[{\"id\":1,\"title\":\"A\",\"status\":\"completed\",\"alert_at\":null,"
                       "\"created_at\":\"" T0 "\"}],\"page\":1,\"page_size\":5,\"has_previous\":false,\"has_next\":false}";
    TEST_ASSERT_EQUAL(BRIDGE_RESOURCE_INVALID, decode_page(BRIDGE_RESOURCE_TASKS, 1U, 200, done));
}

static void alert_page_reads_schedule(void)
{
    const char *body = "{\"items\":[{\"id\":7,\"message\":\"Call\",\"scheduled_at\":\"" T1 "\",\"created_at\":\"" T0 "\"}],"
                       "\"page\":1,\"page_size\":5,\"has_previous\":false,\"has_next\":false}";
    TEST_ASSERT_EQUAL(BRIDGE_RESOURCE_OK, decode_page(BRIDGE_RESOURCE_ALERTS, 1U, 200, body));
    TEST_ASSERT_EQUAL_STRING("Call", page.entries[0].text);
    TEST_ASSERT_EQUAL_STRING(T1, page.entries[0].when_at);
}

static void empty_page_is_valid_and_has_no_navigation_guess(void)
{
    const char *body = "{\"items\":[],\"page\":3,\"page_size\":5,\"has_previous\":true,\"has_next\":false}";
    TEST_ASSERT_EQUAL(BRIDGE_RESOURCE_OK, decode_page(BRIDGE_RESOURCE_NOTES, 3U, 200, body));
    TEST_ASSERT_EQUAL_UINT8(0U, page.count);
    TEST_ASSERT_TRUE(page.has_previous);
}

static void full_page_may_announce_the_next_one(void)
{
    char body[1024];
    size_t used = (size_t)snprintf(body, sizeof(body), "{\"items\":[");
    for (unsigned i = 1; i <= 5U; ++i)
        used += (size_t)snprintf(body + used, sizeof(body) - used, "%s{\"id\":%u,\"title\":\"T%u\",\"created_at\":\"" T0 "\"}",
                                 i > 1U ? "," : "", i, i);
    snprintf(body + used, sizeof(body) - used, "],\"page\":1,\"page_size\":5,\"has_previous\":false,\"has_next\":true}");
    TEST_ASSERT_EQUAL(BRIDGE_RESOURCE_OK, decode_page(BRIDGE_RESOURCE_NOTES, 1U, 200, body));
    TEST_ASSERT_EQUAL_UINT8(5U, page.count);
    TEST_ASSERT_TRUE(page.has_next);
    TEST_ASSERT_EQUAL_STRING("T5", page.entries[4].text);
}

static void page_schema_violations_are_invalid(void)
{
    const char *item = "{\"id\":1,\"title\":\"A\",\"created_at\":\"" T0 "\"}";
    char body[512];
#define CASE(format, ...) \
    do { \
        snprintf(body, sizeof(body), format, ##__VA_ARGS__); \
        TEST_ASSERT_EQUAL_MESSAGE(BRIDGE_RESOURCE_INVALID, decode_page(BRIDGE_RESOURCE_NOTES, 1U, 200, body), body); \
    } while (0)
    CASE("{\"items\":[%s],\"page\":2,\"page_size\":5,\"has_previous\":false,\"has_next\":false}", item);
    CASE("{\"items\":[%s],\"page\":1,\"page_size\":10,\"has_previous\":false,\"has_next\":false}", item);
    CASE("{\"items\":[%s],\"page\":1,\"page_size\":5,\"has_previous\":true,\"has_next\":false}", item);
    CASE("{\"items\":[%s],\"page\":1,\"page_size\":5,\"has_previous\":false,\"has_next\":true}", item);
    CASE("{\"items\":[%s],\"page\":1,\"page_size\":5,\"has_previous\":false}", item);
    CASE("{\"items\":[%s],\"page\":1,\"page_size\":5,\"has_previous\":false,\"has_next\":false,\"total\":1}", item);
    CASE("{\"items\":[%s,%s],\"page\":1,\"page_size\":5,\"has_previous\":false,\"has_next\":false}", item, item);
    CASE("{\"items\":{},\"page\":1,\"page_size\":5,\"has_previous\":false,\"has_next\":false}");
    CASE("{\"items\":[],\"page\":1.5,\"page_size\":5,\"has_previous\":false,\"has_next\":false}");
    CASE("{\"items\":[],\"page\":1,\"page_size\":5,\"has_previous\":0,\"has_next\":false}");
    CASE("{\"items\":[{\"id\":1,\"title\":\"A\",\"created_at\":\"2026-11-02 12:00:00\"}],\"page\":1,\"page_size\":5,"
         "\"has_previous\":false,\"has_next\":false}");
    CASE("{\"items\":[{\"id\":1,\"title\":\"A\",\"created_at\":\"" T0 "\",\"extra\":1}],\"page\":1,\"page_size\":5,"
         "\"has_previous\":false,\"has_next\":false}");
    CASE("{\"items\":[{\"id\":0,\"title\":\"A\",\"created_at\":\"" T0 "\"}],\"page\":1,\"page_size\":5,"
         "\"has_previous\":false,\"has_next\":false}");
    CASE("{\"items\":[{\"id\":9007199254740993,\"title\":\"A\",\"created_at\":\"" T0 "\"}],\"page\":1,\"page_size\":5,"
         "\"has_previous\":false,\"has_next\":false}");
    CASE("{\"items\":[{\"id\":\"1\",\"title\":\"A\",\"created_at\":\"" T0 "\"}],\"page\":1,\"page_size\":5,"
         "\"has_previous\":false,\"has_next\":false}");
    CASE("{\"items\":[{\"id\":1,\"title\":null,\"created_at\":\"" T0 "\"}],\"page\":1,\"page_size\":5,"
         "\"has_previous\":false,\"has_next\":false}");
    CASE("{\"items\":[{\"id\":1,\"title\":\"A\\u0000B\",\"created_at\":\"" T0 "\"}],\"page\":1,\"page_size\":5,"
         "\"has_previous\":false,\"has_next\":false}");
    CASE("[]");
    CASE("{");
#undef CASE
    const char *bad_utf8 = "{\"items\":[{\"id\":1,\"title\":\"\xff\",\"created_at\":\"" T0 "\"}],\"page\":1,\"page_size\":5,"
                           "\"has_previous\":false,\"has_next\":false}";
    TEST_ASSERT_EQUAL(BRIDGE_RESOURCE_INVALID, decode_page(BRIDGE_RESOURCE_NOTES, 1U, 200, bad_utf8));
    TEST_ASSERT_EQUAL(BRIDGE_RESOURCE_INVALID, bridge_resource_decode_page(BRIDGE_RESOURCE_NOTES, 1U, 200, "", 0U, arena,
                                                                           sizeof(arena), &page));
}

static void alert_and_note_pages_reject_other_field_sets(void)
{
    const char *task_in_notes = "{\"items\":[{\"id\":1,\"title\":\"A\",\"status\":\"pending\",\"alert_at\":null,"
                                "\"created_at\":\"" T0 "\"}],\"page\":1,\"page_size\":5,\"has_previous\":false,\"has_next\":false}";
    TEST_ASSERT_EQUAL(BRIDGE_RESOURCE_INVALID, decode_page(BRIDGE_RESOURCE_NOTES, 1U, 200, task_in_notes));
    TEST_ASSERT_EQUAL(BRIDGE_RESOURCE_INVALID, decode_page(BRIDGE_RESOURCE_ALERTS, 1U, 200, task_in_notes));
}

static void duplicate_ids_are_invalid(void)
{
    const char *body = "{\"items\":[{\"id\":1,\"title\":\"A\",\"created_at\":\"" T0 "\"},"
                       "{\"id\":1,\"title\":\"B\",\"created_at\":\"" T0 "\"}],\"page\":1,\"page_size\":5,"
                       "\"has_previous\":false,\"has_next\":false}";
    TEST_ASSERT_EQUAL(BRIDGE_RESOURCE_INVALID, decode_page(BRIDGE_RESOURCE_NOTES, 1U, 200, body));
}

static void body_and_arena_budgets_fail_explicitly(void)
{
    static char big[BRIDGE_RESOURCE_BODY_MAX_BYTES + 2U];
    memset(big, 'x', sizeof(big));
    TEST_ASSERT_EQUAL(BRIDGE_RESOURCE_TOO_LARGE, bridge_resource_decode_page(BRIDGE_RESOURCE_NOTES, 1U, 200, big,
                                                                             BRIDGE_RESOURCE_BODY_MAX_BYTES + 1U, arena,
                                                                             sizeof(arena), &page));
    TEST_ASSERT_EQUAL(BRIDGE_RESOURCE_TOO_LARGE, bridge_resource_decode_detail(BRIDGE_RESOURCE_NOTES, 1U, 500, big,
                                                                               BRIDGE_RESOURCE_BODY_MAX_BYTES + 1U, arena,
                                                                               sizeof(arena), &detail));
    const char *body = "{\"items\":[{\"id\":1,\"title\":\"0123456789\",\"created_at\":\"" T0 "\"}],\"page\":1,\"page_size\":5,"
                       "\"has_previous\":false,\"has_next\":false}";
    TEST_ASSERT_EQUAL(BRIDGE_RESOURCE_TOO_LARGE, bridge_resource_decode_page(BRIDGE_RESOURCE_NOTES, 1U, 200, body,
                                                                             strlen(body), arena, 10U, &page));
    TEST_ASSERT_EQUAL(BRIDGE_RESOURCE_OK, bridge_resource_decode_page(BRIDGE_RESOURCE_NOTES, 1U, 200, body, strlen(body),
                                                                      arena, 11U, &page));
}

static void a_maximum_body_decodes_within_the_arena(void)
{
    static char body[BRIDGE_RESOURCE_BODY_MAX_BYTES + 1U];
    const char *head = "{\"item\":{\"id\":3,\"title\":\"";
    const char *tail = "\",\"description\":\"d\",\"tag\":\"t\",\"channel_id\":null,\"created_at\":\"" T0
                       "\",\"updated_at\":\"" T0 "\"}}";
    const size_t fill = BRIDGE_RESOURCE_BODY_MAX_BYTES - strlen(head) - strlen(tail);
    memcpy(body, head, strlen(head));
    memset(body + strlen(head), 'a', fill);
    strcpy(body + strlen(head) + fill, tail);
    TEST_ASSERT_EQUAL_size_t(BRIDGE_RESOURCE_BODY_MAX_BYTES, strlen(body));
    TEST_ASSERT_EQUAL(BRIDGE_RESOURCE_OK, decode_detail(BRIDGE_RESOURCE_NOTES, 3U, 200, body));
    TEST_ASSERT_EQUAL_size_t(fill, strlen(detail.text));
}

static void note_detail_has_every_field(void)
{
    const char *body = "{\"item\":{\"id\":3,\"title\":\"T\",\"description\":\"Long\\ntext\",\"tag\":\"work\","
                       "\"channel_id\":\"C1\",\"created_at\":\"" T0 "\",\"updated_at\":\"" T1 "\"}}";
    TEST_ASSERT_EQUAL(BRIDGE_RESOURCE_OK, decode_detail(BRIDGE_RESOURCE_NOTES, 3U, 200, body));
    TEST_ASSERT_EQUAL_UINT64(3U, detail.id);
    TEST_ASSERT_EQUAL_STRING("T", detail.text);
    TEST_ASSERT_EQUAL_STRING("Long\ntext", detail.description);
    TEST_ASSERT_EQUAL_STRING("work", detail.tag);
    TEST_ASSERT_EQUAL_STRING(T0, detail.created_at);
    TEST_ASSERT_EQUAL_STRING(T1, detail.updated_at);
    const char *nulls = "{\"item\":{\"id\":3,\"title\":\"T\",\"description\":\"\",\"tag\":null,\"channel_id\":null,"
                        "\"created_at\":\"" T0 "\",\"updated_at\":\"" T0 "\"}}";
    TEST_ASSERT_EQUAL(BRIDGE_RESOURCE_OK, decode_detail(BRIDGE_RESOURCE_NOTES, 3U, 200, nulls));
    TEST_ASSERT_NULL(detail.tag);
    TEST_ASSERT_EQUAL_STRING("", detail.description);
}

static void task_detail_admits_every_state(void)
{
    static const char *const names[] = {"pending", "in_progress", "completed", "canceled"};
    for (unsigned i = 0; i < 4U; ++i) {
        char body[512];
        snprintf(body, sizeof(body),
                 "{\"item\":{\"id\":8,\"title\":\"T\",\"description\":\"D\",\"tag\":null,\"channel_id\":null,"
                 "\"status\":\"%s\",\"alert_at\":\"" T1 "\",\"created_at\":\"" T0 "\"}}", names[i]);
        TEST_ASSERT_EQUAL(BRIDGE_RESOURCE_OK, decode_detail(BRIDGE_RESOURCE_TASKS, 8U, 200, body));
        TEST_ASSERT_EQUAL(i, detail.task_status);
        TEST_ASSERT_EQUAL_STRING(T1, detail.when_at);
    }
    const char *none = "{\"item\":{\"id\":8,\"title\":\"T\",\"description\":\"D\",\"tag\":null,\"channel_id\":null,"
                       "\"status\":\"pending\",\"alert_at\":null,\"created_at\":\"" T0 "\"}}";
    TEST_ASSERT_EQUAL(BRIDGE_RESOURCE_OK, decode_detail(BRIDGE_RESOURCE_TASKS, 8U, 200, none));
    TEST_ASSERT_EQUAL_STRING("", detail.when_at);
    const char *unknown = "{\"item\":{\"id\":8,\"title\":\"T\",\"description\":\"D\",\"tag\":null,\"channel_id\":null,"
                          "\"status\":\"archived\",\"alert_at\":null,\"created_at\":\"" T0 "\"}}";
    TEST_ASSERT_EQUAL(BRIDGE_RESOURCE_INVALID, decode_detail(BRIDGE_RESOURCE_TASKS, 8U, 200, unknown));
}

static void alert_detail_requires_unsent(void)
{
    const char *body = "{\"item\":{\"id\":5,\"message\":\"Call\",\"scheduled_at\":\"" T1 "\",\"sent\":false,"
                       "\"channel_id\":\"C\",\"created_at\":\"" T0 "\"}}";
    TEST_ASSERT_EQUAL(BRIDGE_RESOURCE_OK, decode_detail(BRIDGE_RESOURCE_ALERTS, 5U, 200, body));
    TEST_ASSERT_EQUAL_STRING("Call", detail.text);
    TEST_ASSERT_NULL(detail.description);
    TEST_ASSERT_EQUAL_STRING(T1, detail.when_at);
    const char *sent = "{\"item\":{\"id\":5,\"message\":\"Call\",\"scheduled_at\":\"" T1 "\",\"sent\":true,"
                       "\"channel_id\":\"C\",\"created_at\":\"" T0 "\"}}";
    TEST_ASSERT_EQUAL(BRIDGE_RESOURCE_INVALID, decode_detail(BRIDGE_RESOURCE_ALERTS, 5U, 200, sent));
}

static void detail_identity_and_shape_must_match(void)
{
    const char *body = "{\"item\":{\"id\":3,\"title\":\"T\",\"description\":\"D\",\"tag\":null,\"channel_id\":null,"
                       "\"created_at\":\"" T0 "\",\"updated_at\":\"" T0 "\"}}";
    TEST_ASSERT_EQUAL(BRIDGE_RESOURCE_OK, decode_detail(BRIDGE_RESOURCE_NOTES, 3U, 200, body));
    TEST_ASSERT_EQUAL(BRIDGE_RESOURCE_INVALID, decode_detail(BRIDGE_RESOURCE_NOTES, 4U, 200, body));
    TEST_ASSERT_EQUAL(BRIDGE_RESOURCE_INVALID, decode_detail(BRIDGE_RESOURCE_TASKS, 3U, 200, body));
    const char *unwrapped = "{\"id\":3,\"title\":\"T\"}";
    TEST_ASSERT_EQUAL(BRIDGE_RESOURCE_INVALID, decode_detail(BRIDGE_RESOURCE_NOTES, 3U, 200, unwrapped));
    const char *truncated = "{\"item\":{\"id\":3,\"title\":\"T\",\"description\":\"D";
    TEST_ASSERT_EQUAL(BRIDGE_RESOURCE_INVALID, decode_detail(BRIDGE_RESOURCE_NOTES, 3U, 200, truncated));
    const char *numeric_tag = "{\"item\":{\"id\":3,\"title\":\"T\",\"description\":\"D\",\"tag\":4,\"channel_id\":null,"
                              "\"created_at\":\"" T0 "\",\"updated_at\":\"" T0 "\"}}";
    TEST_ASSERT_EQUAL(BRIDGE_RESOURCE_INVALID, decode_detail(BRIDGE_RESOURCE_NOTES, 3U, 200, numeric_tag));
    TEST_ASSERT_EQUAL(BRIDGE_RESOURCE_INVALID, decode_detail(BRIDGE_RESOURCE_NOTES, 0U, 200, body));
}

static void errors_are_classified_by_status_and_code(void)
{
#define ERR(status, code) "{\"error\":{\"code\":\"" code "\",\"message\":\"ignored\"}}"
    TEST_ASSERT_EQUAL(BRIDGE_RESOURCE_UNAUTHORIZED, decode_detail(BRIDGE_RESOURCE_NOTES, 1U, 401, ERR(401, "unauthorized")));
    TEST_ASSERT_EQUAL(BRIDGE_RESOURCE_NOT_FOUND, decode_detail(BRIDGE_RESOURCE_NOTES, 1U, 404, ERR(404, "resource_not_found")));
    TEST_ASSERT_EQUAL(BRIDGE_RESOURCE_REJECTED, decode_detail(BRIDGE_RESOURCE_NOTES, 1U, 404, ERR(404, "not_found")));
    TEST_ASSERT_EQUAL(BRIDGE_RESOURCE_REJECTED, decode_page(BRIDGE_RESOURCE_NOTES, 1U, 400, ERR(400, "invalid_request")));
    TEST_ASSERT_EQUAL(BRIDGE_RESOURCE_DENIED, decode_page(BRIDGE_RESOURCE_NOTES, 1U, 502, ERR(502, "bot_auth_rejected")));
    TEST_ASSERT_EQUAL(BRIDGE_RESOURCE_DENIED, decode_page(BRIDGE_RESOURCE_NOTES, 1U, 502, ERR(502, "bot_access_denied")));
    TEST_ASSERT_EQUAL(BRIDGE_RESOURCE_UNAVAILABLE, decode_page(BRIDGE_RESOURCE_NOTES, 1U, 502, ERR(502, "upstream_unavailable")));
    TEST_ASSERT_EQUAL(BRIDGE_RESOURCE_UNAVAILABLE, decode_page(BRIDGE_RESOURCE_NOTES, 1U, 502, ERR(502, "invalid_upstream_response")));
    TEST_ASSERT_EQUAL(BRIDGE_RESOURCE_UNAVAILABLE, decode_page(BRIDGE_RESOURCE_NOTES, 1U, 503, ERR(503, "temporarily_unavailable")));
    TEST_ASSERT_EQUAL(BRIDGE_RESOURCE_UNAVAILABLE, decode_page(BRIDGE_RESOURCE_NOTES, 1U, 500, ERR(500, "internal_error")));
    /* The pair decides: a known code under another status is a different, unknown error. */
    TEST_ASSERT_EQUAL(BRIDGE_RESOURCE_REJECTED, decode_page(BRIDGE_RESOURCE_NOTES, 1U, 503, ERR(503, "unauthorized")));
    TEST_ASSERT_EQUAL(BRIDGE_RESOURCE_REJECTED, decode_page(BRIDGE_RESOURCE_NOTES, 1U, 409, ERR(409, "task_state_changed")));
    TEST_ASSERT_EQUAL(BRIDGE_RESOURCE_INVALID, decode_page(BRIDGE_RESOURCE_NOTES, 1U, 502, "<html>bad gateway</html>"));
    TEST_ASSERT_EQUAL(BRIDGE_RESOURCE_INVALID, decode_page(BRIDGE_RESOURCE_NOTES, 1U, 503, "{\"error\":{\"code\":\"x\"}}"));
    TEST_ASSERT_EQUAL(BRIDGE_RESOURCE_INVALID, decode_page(BRIDGE_RESOURCE_NOTES, 1U, 204, ""));
#undef ERR
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(paths_are_canonical_and_bounded);
    RUN_TEST(note_page_keeps_order_and_texts);
    RUN_TEST(task_page_reads_alert_time_or_null);
    RUN_TEST(alert_page_reads_schedule);
    RUN_TEST(empty_page_is_valid_and_has_no_navigation_guess);
    RUN_TEST(full_page_may_announce_the_next_one);
    RUN_TEST(page_schema_violations_are_invalid);
    RUN_TEST(alert_and_note_pages_reject_other_field_sets);
    RUN_TEST(duplicate_ids_are_invalid);
    RUN_TEST(body_and_arena_budgets_fail_explicitly);
    RUN_TEST(a_maximum_body_decodes_within_the_arena);
    RUN_TEST(note_detail_has_every_field);
    RUN_TEST(task_detail_admits_every_state);
    RUN_TEST(alert_detail_requires_unsent);
    RUN_TEST(detail_identity_and_shape_must_match);
    RUN_TEST(errors_are_classified_by_status_and_code);
    return UNITY_END();
}
