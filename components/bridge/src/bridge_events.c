/* Event response validation adapted from event_codec.c in the author's V4 firmware; see THIRD_PARTY_NOTICES.md. */
#include "bridge_events.h"
#include "bridge_text.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "bridge_uuid.h"

/* Dates are kept in their complete UTC wire form for Snooze correlation. */
static unsigned digits(const char *text, size_t count)
{
    unsigned value = 0;
    for (size_t i = 0; i < count; ++i) value = value * 10U + (unsigned)(text[i] - '0');
    return value;
}

static bool timestamp_valid(const char *text)
{
    const size_t length = strnlen(text, BRIDGE_EVENT_TIME_SIZE);
    if (length < 20U || length > 30U || text[4] != '-' || text[7] != '-' || text[10] != 'T' ||
        text[13] != ':' || text[16] != ':' || text[length - 1U] != 'Z') return false;
    for (size_t i = 0; i < length - 1U; ++i) {
        if (i == 4U || i == 7U || i == 10U || i == 13U || i == 16U) continue;
        if (i == 19U && length != 20U) {
            if (text[i] != '.' || length < 22U) return false;
            continue;
        }
        if (text[i] < '0' || text[i] > '9') return false;
    }
    const unsigned year = digits(text, 4U), month = digits(text + 5U, 2U), day = digits(text + 8U, 2U);
    static const unsigned days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month < 1U || month > 12U || day < 1U) return false;
    unsigned limit = days[month - 1U];
    if (month == 2U && year % 4U == 0U && (year % 100U != 0U || year % 400U == 0U)) ++limit;
    return day <= limit && digits(text + 11U, 2U) < 24U && digits(text + 14U, 2U) < 60U &&
           digits(text + 17U, 2U) < 60U;
}

bool bridge_event_valid(const bridge_event_t *event)
{
    if (event == NULL) return false;
    const size_t length = strnlen(event->message, sizeof(event->message));
    return bridge_uuid_valid(event->event_id) && event->alert_id > 0U &&
           event->alert_id <= UINT64_C(9007199254740991) && length <= BRIDGE_EVENT_MESSAGE_MAX_BYTES &&
           bridge_utf8_valid(event->message, length) && timestamp_valid(event->scheduled_at) &&
           timestamp_valid(event->triggered_at);
}

static bool fields_exact(const cJSON *object, const char *const *names, size_t count)
{
    if (!cJSON_IsObject(object) || cJSON_GetArraySize(object) != (int)count) return false;
    unsigned seen = 0;
    const cJSON *item;
    cJSON_ArrayForEach(item, object) {
        size_t field;
        for (field = 0; field < count; ++field)
            if (item->string && strcmp(item->string, names[field]) == 0) break;
        if (field == count || (seen & (1U << field))) return false;
        seen |= 1U << field;
    }
    return seen == (1U << count) - 1U;
}

static bool digit(char c)
{
    return c >= '0' && c <= '9';
}

/* JSON numbers are parsed by cJSON as double. Check the decimal token first:
 * rounding must not turn a fractional/unsafe alert_id into a valid integer.
 * The only numeric value admitted by this response schema is alert_id. */
static bool integer_token(const char *text, size_t length, size_t *end)
{
    size_t i = *end, start = i;
    if (i >= length || !digit(text[i])) return false;
    if (text[i] == '0') {
        ++i;
        if (i < length && digit(text[i])) return false;
    } else {
        while (i < length && digit(text[i])) ++i;
    }
    size_t fraction = 0;
    if (i < length && text[i] == '.') {
        size_t first = ++i;
        while (i < length && digit(text[i])) ++i;
        fraction = i - first;
        if (!fraction) return false;
    }
    size_t coefficient_end = i;
    int exponent = 0;
    if (i < length && (text[i] == 'e' || text[i] == 'E')) {
        ++i;
        bool negative = i < length && text[i] == '-';
        if (i < length && (text[i] == '+' || text[i] == '-')) ++i;
        size_t first = i;
        while (i < length && digit(text[i])) {
            /* Saturation exceeds every possible coefficient in a 4 KiB body. */
            if (exponent < 10000) exponent = exponent * 10 + text[i] - '0';
            ++i;
        }
        if (i == first) return false;
        if (negative) exponent = -exponent;
    }
    if (i < length && text[i] != ',' && text[i] != '}' && text[i] != ']' &&
        text[i] != ' ' && text[i] != '\t' && text[i] != '\r' && text[i] != '\n') return false;

    size_t first_nonzero = coefficient_end, last_nonzero = start, trailing = 0;
    for (size_t j = start; j < coefficient_end; ++j) {
        if (text[j] == '.') continue;
        if (text[j] != '0') {
            if (first_nonzero == coefficient_end) first_nonzero = j;
            last_nonzero = j;
            trailing = 0;
        } else ++trailing;
    }
    if (first_nonzero == coefficient_end) return false;
    int power = exponent - (int)fraction + (int)trailing;
    if (power < 0 || power > 15) return false;
    uint64_t value = 0;
    for (size_t j = first_nonzero; j <= last_nonzero; ++j) {
        if (text[j] == '.') continue;
        if (value > (UINT64_C(9007199254740991) - (unsigned)(text[j] - '0')) / 10)
            return false;
        value = value * 10 + (unsigned)(text[j] - '0');
    }
    while (power--) {
        if (value > UINT64_C(9007199254740991) / 10) return false;
        value *= 10;
    }
    *end = i;
    return true;
}

static bool json_bounded(const char *body, size_t length)
{
    unsigned depth = 0;
    bool quoted = false;
    for (size_t i = 0; i < length; ++i) {
        unsigned char c = (unsigned char)body[i];
        if (quoted) {
            if (c < 0x20) return false;
            if (c == '\\') {
                if (++i >= length) return false;
                /* cJSON strings have no decoded length. Reject escaped NUL
                 * before parsing, including property names and metadata. */
                if (body[i] == 'u' && length - i >= 5 &&
                    memcmp(body + i + 1, "0000", 4) == 0) return false;
            } else if (c == '"') quoted = false;
        } else if (c == '"') quoted = true;
        else if (c == '{' || c == '[') {
            if (++depth > 4) return false;
        } else if (c == '}' || c == ']') {
            if (!depth) return false;
            --depth;
        } else if (digit((char)c) || c == '-' || c == '.') {
            size_t end = i;
            if (!integer_token(body, length, &end)) return false;
            i = end - 1;
        } else if (c < 0x20 && c != '\t' && c != '\r' && c != '\n') return false;
    }
    return !quoted && depth == 0;
}

static bool copy_string(const cJSON *item, char *target, size_t capacity)
{
    if (!cJSON_IsString(item) || !item->valuestring || strlen(item->valuestring) >= capacity)
        return false;
    memcpy(target, item->valuestring, strlen(item->valuestring) + 1);
    return true;
}

bridge_event_decode_t bridge_event_decode(const char *body, size_t length, int http_status,
                                                  bridge_event_t *event, bool *present)
{
    if (!body || !event || !present || http_status != 200 || !length) return BRIDGE_EVENT_DECODE_INVALID;
    if (length > BRIDGE_EVENT_RESPONSE_MAX_BYTES) return BRIDGE_EVENT_DECODE_TOO_LARGE;
    if (memchr(body, '\0', length) || !json_bounded(body, length)) return BRIDGE_EVENT_DECODE_INVALID;
    char *terminated = malloc(length + 1);
    if (!terminated) return BRIDGE_EVENT_DECODE_INVALID;
    memcpy(terminated, body, length);
    terminated[length] = '\0';
    if (!bridge_utf8_valid(terminated, length)) {
        free(terminated);
        return BRIDGE_EVENT_DECODE_INVALID;
    }
    cJSON *root = cJSON_ParseWithLengthOpts(terminated, length + 1, NULL, true);
    free(terminated);
    if (!root) return BRIDGE_EVENT_DECODE_INVALID;
    const char *const wrapper_fields[] = {"events"};
    const char *const event_fields[] = {"event_id", "type", "alert_id", "message", "scheduled_at", "triggered_at"};
    const cJSON *events = cJSON_GetObjectItemCaseSensitive(root, "events");
    bool valid = fields_exact(root, wrapper_fields, 1) && cJSON_IsArray(events) &&
                 cJSON_GetArraySize(events) <= 1;
    bool decoded_present = valid && cJSON_GetArraySize(events) == 1;
    bridge_event_t decoded = {0};
    bool too_large = false;
    if (decoded_present) {
        const cJSON *item = cJSON_GetArrayItem(events, 0);
        const cJSON *alert = cJSON_GetObjectItemCaseSensitive(item, "alert_id");
        valid = fields_exact(item, event_fields, 6) && cJSON_IsNumber(alert) &&
                isfinite(alert->valuedouble) && alert->valuedouble >= 1 &&
                alert->valuedouble <= 9007199254740991.0 &&
                floor(alert->valuedouble) == alert->valuedouble;
        const cJSON *message = cJSON_GetObjectItemCaseSensitive(item, "message");
        if (valid && cJSON_IsString(message) && message->valuestring &&
            strlen(message->valuestring) >= BRIDGE_EVENT_MESSAGE_SIZE) {
            too_large = true;
            valid = false;
        }
        if (valid) {
            valid = cJSON_IsString(cJSON_GetObjectItemCaseSensitive(item, "type")) &&
                    strcmp(cJSON_GetObjectItemCaseSensitive(item, "type")->valuestring, "alert.triggered") == 0;
        }
        if (valid) {
            decoded.alert_id = (uint64_t)alert->valuedouble;
            valid = copy_string(cJSON_GetObjectItemCaseSensitive(item, "event_id"),
                                decoded.event_id, sizeof(decoded.event_id)) &&
                    copy_string(cJSON_GetObjectItemCaseSensitive(item, "message"),
                                decoded.message, sizeof(decoded.message)) &&
                    copy_string(cJSON_GetObjectItemCaseSensitive(item, "scheduled_at"),
                                decoded.scheduled_at, sizeof(decoded.scheduled_at)) &&
                    copy_string(cJSON_GetObjectItemCaseSensitive(item, "triggered_at"),
                                decoded.triggered_at, sizeof(decoded.triggered_at)) &&
                    bridge_event_valid(&decoded);
        }
    }
    if (valid) {
        *event = decoded;
        *present = decoded_present;
    }
    cJSON_Delete(root);
    return valid ? BRIDGE_EVENT_DECODE_OK : too_large ? BRIDGE_EVENT_DECODE_TOO_LARGE : BRIDGE_EVENT_DECODE_INVALID;
}

bool bridge_event_ack_succeeded(int http_status, size_t body_length)
{
    return http_status == 204 && body_length == 0;
}
