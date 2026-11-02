#include <stdio.h>
#include <string.h>

#include "bridge_assistant.h"
#include "unity.h"

#define ID "12345678-1234-4abc-9876-123456789abc"
#define OTHER_ID "12345678-1234-4abc-9876-123456789abd"
#define URL "\"/v1/device/assistant-requests/" ID "\""

static char message[BRIDGE_ASSISTANT_MESSAGE_SIZE];
static bridge_assistant_result_t result;
static char body[BRIDGE_ASSISTANT_JSON_MAX_BYTES + 2U];

void setUp(void)
{
    memset(message, 'x', sizeof(message));
    memset(&result, 0x5A, sizeof(result));
}
void tearDown(void) {}

static bridge_assistant_decode_t decode(const char *json, int status, bool upload)
{
    return bridge_assistant_decode(json, strlen(json), status, upload, ID, &result, message, sizeof(message));
}

static void assert_cleared(void)
{
    TEST_ASSERT_EQUAL_INT(0, message[0]);
    TEST_ASSERT_EQUAL_INT(BRIDGE_ASSISTANT_PROCESSING, result.status);
    TEST_ASSERT_FALSE(result.content_omitted);
    TEST_ASSERT_EQUAL_INT(0, result.error_code[0]);
}

static void valid_statuses_decode(void)
{
    const char *processing = "{\"request_id\":\"" ID "\",\"status\":\"processing\",\"status_url\":" URL
                             ",\"message\":null,\"content_omitted\":false,\"error\":null}";
    TEST_ASSERT_EQUAL_INT(BRIDGE_ASSISTANT_DECODE_OK, decode(processing, 202, true));
    TEST_ASSERT_EQUAL_INT(BRIDGE_ASSISTANT_PROCESSING, result.status);
    TEST_ASSERT_EQUAL_STRING("", message);
    TEST_ASSERT_EQUAL_INT(BRIDGE_ASSISTANT_DECODE_OK, decode(processing, 200, false));

    const char *completed = " {\"request_id\":\"" ID "\",\"status\":\"completed\",\"status_url\":" URL
                            ",\"message\":\"Listo \\u2014 agendado.\\nOk\",\"content_omitted\":true,\"error\":null}\n";
    TEST_ASSERT_EQUAL_INT(BRIDGE_ASSISTANT_DECODE_OK, decode(completed, 200, true));
    TEST_ASSERT_EQUAL_INT(BRIDGE_ASSISTANT_COMPLETED, result.status);
    TEST_ASSERT_TRUE(result.content_omitted);
    TEST_ASSERT_EQUAL_STRING("Listo \xE2\x80\x94 agendado.\nOk", message);

    const char *failed = "{\"request_id\":\"" ID "\",\"status\":\"failed\",\"status_url\":" URL
                         ",\"message\":\"Could not transcribe.\",\"content_omitted\":false,"
                         "\"error\":{\"code\":\"transcription_failed\"}}";
    TEST_ASSERT_EQUAL_INT(BRIDGE_ASSISTANT_DECODE_OK, decode(failed, 200, false));
    TEST_ASSERT_EQUAL_INT(BRIDGE_ASSISTANT_FAILED, result.status);
    TEST_ASSERT_EQUAL_STRING("transcription_failed", result.error_code);

    const char *unknown = "{\"request_id\":\"" ID "\",\"status\":\"unknown\",\"status_url\":" URL
                          ",\"message\":\"Unconfirmed.\",\"content_omitted\":false,"
                          "\"error\":{\"code\":\"result_unconfirmed\"}}";
    TEST_ASSERT_EQUAL_INT(BRIDGE_ASSISTANT_DECODE_OK, decode(unknown, 200, false));
    TEST_ASSERT_EQUAL_INT(BRIDGE_ASSISTANT_UNKNOWN, result.status);
}

static void http_status_rules(void)
{
    const char *completed = "{\"request_id\":\"" ID "\",\"status\":\"completed\",\"status_url\":" URL
                            ",\"message\":\"Ok\",\"content_omitted\":false,\"error\":null}";
    TEST_ASSERT_EQUAL_INT(BRIDGE_ASSISTANT_DECODE_INVALID, decode(completed, 202, true));
    assert_cleared();
    TEST_ASSERT_EQUAL_INT(BRIDGE_ASSISTANT_DECODE_HTTP_ERROR, decode(completed, 202, false));
    TEST_ASSERT_EQUAL_INT(BRIDGE_ASSISTANT_DECODE_HTTP_ERROR, decode(completed, 404, false));
    TEST_ASSERT_EQUAL_INT(BRIDGE_ASSISTANT_DECODE_HTTP_ERROR, decode(completed, 409, true));
    TEST_ASSERT_EQUAL_INT(BRIDGE_ASSISTANT_DECODE_HTTP_ERROR, decode(completed, 503, false));
    assert_cleared();
}

static void schema_violations_are_rejected(void)
{
    static const char *const cases[] = {
        /* Extra, duplicate and missing fields. */
        "{\"request_id\":\"" ID "\",\"status\":\"processing\",\"status_url\":" URL
        ",\"message\":null,\"content_omitted\":false,\"error\":null,\"extra\":1}",
        "{\"request_id\":\"" ID "\",\"status\":\"processing\",\"status\":\"processing\",\"status_url\":" URL
        ",\"message\":null,\"content_omitted\":false}",
        "{\"request_id\":\"" ID "\",\"status\":\"processing\",\"status_url\":" URL ",\"message\":null,\"error\":null}",
        /* Another ID, URL or status. */
        "{\"request_id\":\"" OTHER_ID "\",\"status\":\"processing\",\"status_url\":" URL
        ",\"message\":null,\"content_omitted\":false,\"error\":null}",
        "{\"request_id\":\"" ID "\",\"status\":\"processing\",\"status_url\":\"https://x/v1/device/assistant-requests/" ID
        "\",\"message\":null,\"content_omitted\":false,\"error\":null}",
        "{\"request_id\":\"" ID "\",\"status\":\"done\",\"status_url\":" URL
        ",\"message\":null,\"content_omitted\":false,\"error\":null}",
        /* Field rules per status. */
        "{\"request_id\":\"" ID "\",\"status\":\"processing\",\"status_url\":" URL
        ",\"message\":\"x\",\"content_omitted\":false,\"error\":null}",
        "{\"request_id\":\"" ID "\",\"status\":\"processing\",\"status_url\":" URL
        ",\"message\":null,\"content_omitted\":true,\"error\":null}",
        "{\"request_id\":\"" ID "\",\"status\":\"completed\",\"status_url\":" URL
        ",\"message\":\"Ok\",\"content_omitted\":false,\"error\":{\"code\":\"x\"}}",
        "{\"request_id\":\"" ID "\",\"status\":\"completed\",\"status_url\":" URL
        ",\"message\":\" \\n\",\"content_omitted\":false,\"error\":null}",
        "{\"request_id\":\"" ID "\",\"status\":\"failed\",\"status_url\":" URL
        ",\"message\":\"x\",\"content_omitted\":true,\"error\":{\"code\":\"x\"}}",
        "{\"request_id\":\"" ID "\",\"status\":\"failed\",\"status_url\":" URL
        ",\"message\":\"x\",\"content_omitted\":false,\"error\":{\"code\":\"bad code\"}}",
        "{\"request_id\":\"" ID "\",\"status\":\"failed\",\"status_url\":" URL
        ",\"message\":\"x\",\"content_omitted\":false,\"error\":{\"code\":\"x\",\"detail\":\"y\"}}",
        "{\"request_id\":\"" ID "\",\"status\":\"unknown\",\"status_url\":" URL
        ",\"message\":\"x\",\"content_omitted\":false,\"error\":{\"code\":\"interrupted\"}}",
        "{\"request_id\":\"" ID "\",\"status\":\"completed\",\"status_url\":" URL
        ",\"message\":\"Ok\",\"content_omitted\":\"no\",\"error\":null}",
        /* Text and framing. */
        "{\"request_id\":\"" ID "\",\"status\":\"completed\",\"status_url\":" URL
        ",\"message\":\"a\\u0000b\",\"content_omitted\":false,\"error\":null}",
        "{\"request_id\":\"" ID "\",\"status\":\"completed\",\"status_url\":" URL
        ",\"message\":\"\xC3\x28\",\"content_omitted\":false,\"error\":null}",
        "{\"request_id\":\"" ID "\",\"status\":\"completed\",\"status_url\":" URL
        ",\"message\":\"\\ud800\",\"content_omitted\":false,\"error\":null}",
        "{\"request_id\":\"" ID "\",\"status\":\"completed\",\"status_url\":" URL
        ",\"message\":\"Ok\",\"content_omitted\":false,\"error\":null} x",
        "[{\"request_id\":\"" ID "\"}]",
        "{\"request_id\":\"" ID "\",\"status\":\"completed\"",
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        char label[16];
        snprintf(label, sizeof(label), "case %u", (unsigned)i);
        setUp();
        TEST_ASSERT_EQUAL_INT_MESSAGE(BRIDGE_ASSISTANT_DECODE_INVALID, decode(cases[i], 200, false), label);
        assert_cleared();
    }
}

static int build_completed(size_t text_bytes)
{
    int length = snprintf(body, sizeof(body), "{\"request_id\":\"" ID "\",\"status\":\"completed\",\"status_url\":" URL
                                              ",\"message\":\"");
    memset(body + length, 'a', text_bytes);
    length += (int)text_bytes;
    length += snprintf(body + length, sizeof(body) - (size_t)length, "\",\"content_omitted\":false,\"error\":null}");
    return length;
}

static void sizes_are_bounded_without_truncation(void)
{
    int length = build_completed(BRIDGE_ASSISTANT_MESSAGE_MAX_BYTES);
    TEST_ASSERT_EQUAL_INT(BRIDGE_ASSISTANT_DECODE_OK, bridge_assistant_decode(body, (size_t)length, 200, false, ID,
                                                                              &result, message, sizeof(message)));
    TEST_ASSERT_EQUAL_UINT(BRIDGE_ASSISTANT_MESSAGE_MAX_BYTES, strlen(message));
    length = build_completed(BRIDGE_ASSISTANT_MESSAGE_MAX_BYTES + 1U);
    TEST_ASSERT_EQUAL_INT(BRIDGE_ASSISTANT_DECODE_TOO_LARGE, bridge_assistant_decode(body, (size_t)length, 200, false,
                                                                                     ID, &result, message,
                                                                                     sizeof(message)));
    assert_cleared();
    TEST_ASSERT_EQUAL_INT(BRIDGE_ASSISTANT_DECODE_TOO_LARGE,
                          bridge_assistant_decode(body, BRIDGE_ASSISTANT_JSON_MAX_BYTES + 1U, 200, false, ID, &result,
                                                  message, sizeof(message)));
    TEST_ASSERT_EQUAL_INT(BRIDGE_ASSISTANT_DECODE_INVALID,
                          bridge_assistant_decode(body, (size_t)length, 200, false, ID, &result, message,
                                                  BRIDGE_ASSISTANT_MESSAGE_MAX_BYTES));
}

static void error_code_length_is_bounded(void)
{
    char json[512];
    char code[BRIDGE_ASSISTANT_ERROR_CODE_MAX_BYTES + 2U];
    memset(code, 'c', sizeof(code));
    code[BRIDGE_ASSISTANT_ERROR_CODE_MAX_BYTES] = '\0';
    snprintf(json, sizeof(json), "{\"request_id\":\"" ID "\",\"status\":\"failed\",\"status_url\":" URL
                                 ",\"message\":\"x\",\"content_omitted\":false,\"error\":{\"code\":\"%s\"}}", code);
    TEST_ASSERT_EQUAL_INT(BRIDGE_ASSISTANT_DECODE_OK, decode(json, 200, false));
    code[BRIDGE_ASSISTANT_ERROR_CODE_MAX_BYTES] = 'c';
    code[BRIDGE_ASSISTANT_ERROR_CODE_MAX_BYTES + 1U] = '\0';
    snprintf(json, sizeof(json), "{\"request_id\":\"" ID "\",\"status\":\"failed\",\"status_url\":" URL
                                 ",\"message\":\"x\",\"content_omitted\":false,\"error\":{\"code\":\"%s\"}}", code);
    TEST_ASSERT_EQUAL_INT(BRIDGE_ASSISTANT_DECODE_INVALID, decode(json, 200, false));
}

static void path_requires_a_canonical_id(void)
{
    char path[BRIDGE_ASSISTANT_PATH_SIZE];
    TEST_ASSERT_TRUE(bridge_assistant_path(ID, path, sizeof(path)));
    TEST_ASSERT_EQUAL_STRING("/v1/device/assistant-requests/" ID, path);
    TEST_ASSERT_FALSE(bridge_assistant_path("../events", path, sizeof(path)));
    TEST_ASSERT_FALSE(bridge_assistant_path(ID, path, sizeof(path) - 1U));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(valid_statuses_decode);
    RUN_TEST(http_status_rules);
    RUN_TEST(schema_violations_are_rejected);
    RUN_TEST(sizes_are_bounded_without_truncation);
    RUN_TEST(error_code_length_is_bounded);
    RUN_TEST(path_requires_a_canonical_id);
    return UNITY_END();
}
