/* Extracted from event_codec.c in the author's V4 firmware; see THIRD_PARTY_NOTICES.md. */
#include "bridge_json.h"

#include <stdint.h>
#include <string.h>

bool bridge_json_fields_exact(const cJSON *object, const char *const *names, size_t count)
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
 * Every number admitted by the bridge schemas is a positive safe integer. */
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

bool bridge_json_guard(const char *body, size_t length, unsigned max_depth)
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
            if (++depth > max_depth) return false;
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
