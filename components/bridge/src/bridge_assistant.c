/* Adapted from main/assistant_codec.c of the author's previous firmware (V4); see THIRD_PARTY_NOTICES.md. */
#include "bridge_assistant.h"

#include <stdio.h>
#include <string.h>

#include "bridge_text.h"
#include "cJSON.h"

#define HTTP_OK 200
#define HTTP_ACCEPTED 202
/* The root object and its error object; six root members plus the error code. */
#define RESPONSE_MAX_OBJECTS 2U
#define RESPONSE_MAX_MEMBERS 7U
#define RESPONSE_FIELD_COUNT 6U
#define UNKNOWN_ERROR_CODE "result_unconfirmed"

static const char *const RESPONSE_FIELDS[RESPONSE_FIELD_COUNT] = {
    "request_id", "status", "status_url", "message", "content_omitted", "error",
};

bool bridge_assistant_path(const char *id, char *path, size_t capacity)
{
    if (!bridge_uuid_valid(id) || path == NULL) {
        return false;
    }
    const int length = snprintf(path, capacity, BRIDGE_ASSISTANT_ROUTE_PREFIX "%s", id);
    return length > 0 && (size_t)length < capacity;
}

/* Exactly the named members, each once. */
static bool fields_exact(const cJSON *object, const char *const *names, size_t count)
{
    if (!cJSON_IsObject(object) || cJSON_GetArraySize(object) != (int)count) {
        return false;
    }
    unsigned seen = 0;
    const cJSON *field;
    cJSON_ArrayForEach(field, object) {
        size_t i = 0;
        while (i < count && (field->string == NULL || strcmp(field->string, names[i]) != 0)) {
            ++i;
        }
        if (i == count || (seen & (1U << i)) != 0U) {
            return false;
        }
        seen |= 1U << i;
    }
    return seen == (1U << count) - 1U;
}

static bool parse_status(const cJSON *item, bridge_assistant_status_t *status)
{
    static const char *const names[] = {"processing", "completed", "failed", "unknown"};
    if (!cJSON_IsString(item)) {
        return false;
    }
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        if (strcmp(item->valuestring, names[i]) == 0) {
            *status = (bridge_assistant_status_t)i;
            return true;
        }
    }
    return false;
}

static bool status_url_matches(const cJSON *url, const char *id)
{
    char expected[BRIDGE_ASSISTANT_PATH_SIZE];
    return cJSON_IsString(url) && bridge_assistant_path(id, expected, sizeof(expected)) &&
           strcmp(url->valuestring, expected) == 0;
}

/* Valid UTF-8 with something besides whitespace and no control characters other than line breaks and tabs. */
static bool useful_text(const char *text, size_t length)
{
    if (!bridge_utf8_valid(text, length)) {
        return false;
    }
    bool useful = false;
    for (size_t i = 0; i < length; ++i) {
        const unsigned char c = (unsigned char)text[i];
        if ((c < 0x20U && c != '\n' && c != '\r' && c != '\t') || c == 0x7FU) {
            return false;
        }
        if (c != ' ' && c != '\n' && c != '\r' && c != '\t') {
            useful = true;
        }
    }
    return useful;
}

/* An error object with only a safe code: 1-64 letters, digits, '_', '-' or '.'. */
static bool parse_error_code(const cJSON *error, char code[BRIDGE_ASSISTANT_ERROR_CODE_MAX_BYTES + 1U])
{
    static const char *const fields[] = {"code"};
    if (!fields_exact(error, fields, 1U)) {
        return false;
    }
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(error, "code");
    if (!cJSON_IsString(item)) {
        return false;
    }
    const size_t length = strlen(item->valuestring);
    if (length == 0U || length > BRIDGE_ASSISTANT_ERROR_CODE_MAX_BYTES) {
        return false;
    }
    for (size_t i = 0; i < length; ++i) {
        const char c = item->valuestring[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' ||
              c == '.')) {
            return false;
        }
    }
    memcpy(code, item->valuestring, length + 1U);
    return true;
}

static bool only_whitespace(const char *at, const char *end)
{
    for (; at < end; ++at) {
        if (*at != ' ' && *at != '\r' && *at != '\n' && *at != '\t') {
            return false;
        }
    }
    return true;
}

/* message, content_omitted and error as the status requires; the terminal text is returned through text. */
static bridge_assistant_decode_t decode_outcome(const cJSON *root, bridge_assistant_result_t *result,
                                                const char **text)
{
    const cJSON *message = cJSON_GetObjectItemCaseSensitive(root, "message");
    const cJSON *omitted = cJSON_GetObjectItemCaseSensitive(root, "content_omitted");
    const cJSON *error = cJSON_GetObjectItemCaseSensitive(root, "error");
    if (!cJSON_IsBool(omitted)) {
        return BRIDGE_ASSISTANT_DECODE_INVALID;
    }
    result->content_omitted = cJSON_IsTrue(omitted);
    if (result->status == BRIDGE_ASSISTANT_PROCESSING) {
        return cJSON_IsNull(message) && cJSON_IsNull(error) && !result->content_omitted
                   ? BRIDGE_ASSISTANT_DECODE_OK
                   : BRIDGE_ASSISTANT_DECODE_INVALID;
    }
    if (!cJSON_IsString(message)) {
        return BRIDGE_ASSISTANT_DECODE_INVALID;
    }
    const size_t length = strlen(message->valuestring);
    if (length > BRIDGE_ASSISTANT_MESSAGE_MAX_BYTES) {
        return BRIDGE_ASSISTANT_DECODE_TOO_LARGE;
    }
    if (!useful_text(message->valuestring, length)) {
        return BRIDGE_ASSISTANT_DECODE_INVALID;
    }
    if (result->status == BRIDGE_ASSISTANT_COMPLETED) {
        if (!cJSON_IsNull(error)) {
            return BRIDGE_ASSISTANT_DECODE_INVALID;
        }
    } else if (result->content_omitted || !parse_error_code(error, result->error_code) ||
               (result->status == BRIDGE_ASSISTANT_UNKNOWN && strcmp(result->error_code, UNKNOWN_ERROR_CODE) != 0)) {
        return BRIDGE_ASSISTANT_DECODE_INVALID;
    }
    *text = message->valuestring;
    return BRIDGE_ASSISTANT_DECODE_OK;
}

static bridge_assistant_decode_t decode_root(const cJSON *root, int http_status, const char *expected_id,
                                             bridge_assistant_result_t *result, const char **text)
{
    const cJSON *id = cJSON_GetObjectItemCaseSensitive(root, "request_id");
    if (!fields_exact(root, RESPONSE_FIELDS, RESPONSE_FIELD_COUNT) || !cJSON_IsString(id) ||
        strcmp(id->valuestring, expected_id) != 0 ||
        !parse_status(cJSON_GetObjectItemCaseSensitive(root, "status"), &result->status) ||
        !status_url_matches(cJSON_GetObjectItemCaseSensitive(root, "status_url"), expected_id)) {
        return BRIDGE_ASSISTANT_DECODE_INVALID;
    }
    /* 202 is only the admission of a new attempt. */
    if (http_status == HTTP_ACCEPTED && result->status != BRIDGE_ASSISTANT_PROCESSING) {
        return BRIDGE_ASSISTANT_DECODE_INVALID;
    }
    return decode_outcome(root, result, text);
}

bridge_assistant_decode_t bridge_assistant_decode(const char *body, size_t length, int http_status, bool upload,
                                                  const char *expected_id, bridge_assistant_result_t *result,
                                                  char *message, size_t capacity)
{
    if (result != NULL) {
        memset(result, 0, sizeof(*result));
    }
    if (message != NULL && capacity != 0U) {
        message[0] = '\0';
    }
    if (result == NULL || message == NULL || capacity < BRIDGE_ASSISTANT_MESSAGE_SIZE ||
        !bridge_uuid_valid(expected_id)) {
        return BRIDGE_ASSISTANT_DECODE_INVALID;
    }
    if (http_status != HTTP_OK && !(upload && http_status == HTTP_ACCEPTED)) {
        return BRIDGE_ASSISTANT_DECODE_HTTP_ERROR;
    }
    if (body == NULL || length == 0U) {
        return BRIDGE_ASSISTANT_DECODE_INVALID;
    }
    if (length > BRIDGE_ASSISTANT_JSON_MAX_BYTES) {
        return BRIDGE_ASSISTANT_DECODE_TOO_LARGE;
    }
    if (!bridge_json_bounded(body, length, RESPONSE_MAX_OBJECTS, RESPONSE_MAX_MEMBERS)) {
        return BRIDGE_ASSISTANT_DECODE_INVALID;
    }
    /* Parses the caller's buffer in place; trailing bytes are checked here instead of requiring a terminator. */
    const char *end = NULL;
    cJSON *root = cJSON_ParseWithLengthOpts(body, length, &end, false);
    if (root == NULL) {
        return BRIDGE_ASSISTANT_DECODE_INVALID;
    }
    const char *text = NULL;
    bridge_assistant_decode_t decoded = end != NULL && end >= body && end <= body + length &&
                                                only_whitespace(end, body + length)
                                            ? decode_root(root, http_status, expected_id, result, &text)
                                            : BRIDGE_ASSISTANT_DECODE_INVALID;
    if (decoded == BRIDGE_ASSISTANT_DECODE_OK && text != NULL) {
        memcpy(message, text, strlen(text) + 1U);
    }
    cJSON_Delete(root);
    if (decoded != BRIDGE_ASSISTANT_DECODE_OK) {
        memset(result, 0, sizeof(*result));
        message[0] = '\0';
    }
    return decoded;
}
