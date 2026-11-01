#include "provision_line.h"

#include <string.h>

#include "hex_digit.h"

void provision_line_init(provision_line_t *line, char *buffer)
{
    line->buffer = buffer;
    provision_line_clear(line);
}

void provision_line_clear(provision_line_t *line)
{
    memset(line->buffer, 0, PROVISION_LINE_MAX_BYTES + 1U);
    line->length = 0;
    line->rejected = false;
    line->last_byte_ms = 0;
}

void provision_line_expire(provision_line_t *line, uint64_t now_ms)
{
    if ((line->length > 0 || line->rejected) && now_ms - line->last_byte_ms >= PROVISION_LINE_IDLE_MS) {
        provision_line_clear(line);
    }
}

provision_line_status_t provision_line_feed(provision_line_t *line, char byte, uint64_t now_ms)
{
    provision_line_expire(line, now_ms);
    line->last_byte_ms = now_ms;
    if (byte == '\r') {
        return PROVISION_LINE_PENDING;
    }
    if (byte == '\n') {
        if (line->rejected) {
            provision_line_clear(line);
            return PROVISION_LINE_REJECTED;
        }
        line->buffer[line->length] = '\0';
        return PROVISION_LINE_READY;
    }
    if (line->length == PROVISION_LINE_MAX_BYTES || byte == '\0') {
        memset(line->buffer, 0, PROVISION_LINE_MAX_BYTES + 1U);
        line->length = 0;
        line->rejected = true;
    }
    if (!line->rejected) {
        line->buffer[line->length++] = byte;
    }
    return PROVISION_LINE_PENDING;
}

typedef enum {
    FIELD_VERSION,
    FIELD_OP,
    FIELD_WIFI_SSID,
    FIELD_WIFI_PASSWORD,
    FIELD_BRIDGE_URL,
    FIELD_DEVICE_TOKEN,
    FIELD_SCREEN_TIMEOUT,
    FIELD_COUNT,
} field_t;

static const char *const FIELD_NAMES[FIELD_COUNT] = {
    [FIELD_VERSION] = "v",
    [FIELD_OP] = "op",
    [FIELD_WIFI_SSID] = "wifi_ssid",
    [FIELD_WIFI_PASSWORD] = "wifi_password",
    [FIELD_BRIDGE_URL] = "bridge_url",
    [FIELD_DEVICE_TOKEN] = "device_token",
    [FIELD_SCREEN_TIMEOUT] = "screen_timeout_s",
};

#define FIELD_BIT(field) (1U << (field))
#define BRIDGE_FIELDS                                                                                             \
    (FIELD_BIT(FIELD_VERSION) | FIELD_BIT(FIELD_OP) | FIELD_BIT(FIELD_BRIDGE_URL) | FIELD_BIT(FIELD_DEVICE_TOKEN))
#define CONFIGURE_FIELDS                                                                                          \
    (BRIDGE_FIELDS | FIELD_BIT(FIELD_WIFI_SSID) | FIELD_BIT(FIELD_WIFI_PASSWORD) | FIELD_BIT(FIELD_SCREEN_TIMEOUT))

#define OP_CONFIGURE_BRIDGE "configure_bridge"
#define OP_CONFIGURE "configure"

#define UTF8_ONE_BYTE_MAX 0x7FU
#define UTF8_TWO_BYTES_MAX 0x7FFU
#define UTF8_THREE_BYTES_MAX 0xFFFFU
#define SURROGATE_HIGH_FIRST 0xD800U
#define SURROGATE_HIGH_LAST 0xDBFFU
#define SURROGATE_LOW_FIRST 0xDC00U
#define SURROGATE_LOW_LAST 0xDFFFU
#define SURROGATE_PAIR_BASE 0x10000U
#define SURROGATE_BITS 10U

static void skip_space(char **at)
{
    while (**at == ' ' || **at == '\t' || **at == '\r' || **at == '\n') {
        ++*at;
    }
}

/* Reads the four hex digits after "\u". */
static bool read_unicode_escape(char **at, uint32_t *code)
{
    *code = 0;
    for (unsigned i = 0; i < 4U; ++i) {
        const int digit = hex_digit_value((*at)[i]);
        if (digit < 0) {
            return false;
        }
        *code = (*code << 4) | (uint32_t)digit;
    }
    *at += 4;
    return true;
}

static bool decode_code_point(char **at, uint32_t *code)
{
    if (!read_unicode_escape(at, code) || *code == 0 ||
        (*code >= SURROGATE_LOW_FIRST && *code <= SURROGATE_LOW_LAST)) {
        return false;
    }
    if (*code < SURROGATE_HIGH_FIRST || *code > SURROGATE_HIGH_LAST) {
        return true;
    }
    uint32_t low;
    if ((*at)[0] != '\\' || (*at)[1] != 'u') {
        return false;
    }
    *at += 2;
    if (!read_unicode_escape(at, &low) || low < SURROGATE_LOW_FIRST || low > SURROGATE_LOW_LAST) {
        return false;
    }
    *code = SURROGATE_PAIR_BASE + (((*code - SURROGATE_HIGH_FIRST) << SURROGATE_BITS) | (low - SURROGATE_LOW_FIRST));
    return true;
}

/* The UTF-8 form is never longer than its escape, so writing in place stays behind the reader. */
static void write_utf8(char **out, uint32_t code)
{
    if (code <= UTF8_ONE_BYTE_MAX) {
        *(*out)++ = (char)code;
    } else if (code <= UTF8_TWO_BYTES_MAX) {
        *(*out)++ = (char)(0xC0U | (code >> 6));
        *(*out)++ = (char)(0x80U | (code & 0x3FU));
    } else if (code <= UTF8_THREE_BYTES_MAX) {
        *(*out)++ = (char)(0xE0U | (code >> 12));
        *(*out)++ = (char)(0x80U | ((code >> 6) & 0x3FU));
        *(*out)++ = (char)(0x80U | (code & 0x3FU));
    } else {
        *(*out)++ = (char)(0xF0U | (code >> 18));
        *(*out)++ = (char)(0x80U | ((code >> 12) & 0x3FU));
        *(*out)++ = (char)(0x80U | ((code >> 6) & 0x3FU));
        *(*out)++ = (char)(0x80U | (code & 0x3FU));
    }
}

static bool decode_escape(char **at, char **out)
{
    const char kind = *(*at)++;
    switch (kind) {
    case '"':
    case '\\':
    case '/':
        *(*out)++ = kind;
        return true;
    case 'b':
        *(*out)++ = '\b';
        return true;
    case 'f':
        *(*out)++ = '\f';
        return true;
    case 'n':
        *(*out)++ = '\n';
        return true;
    case 'r':
        *(*out)++ = '\r';
        return true;
    case 't':
        *(*out)++ = '\t';
        return true;
    case 'u': {
        uint32_t code;
        if (!decode_code_point(at, &code)) {
            return false;
        }
        write_utf8(out, code);
        return true;
    }
    default:
        return false;
    }
}

/* *at is on the opening quote. The decoded string is NUL-terminated in place and starts at *value. */
static bool parse_string(char **at, char **value)
{
    if (**at != '"') {
        return false;
    }
    ++*at;
    char *out = *at;
    *value = out;
    for (;;) {
        const unsigned char c = (unsigned char)**at;
        if (c == '"') {
            ++*at;
            *out = '\0';
            return true;
        }
        if (c < 0x20U) {
            return false;
        }
        ++*at;
        if (c == '\\') {
            if (!decode_escape(at, &out)) {
                return false;
            }
        } else {
            *out++ = (char)c;
        }
    }
}

static bool parse_unsigned(char **at, uint32_t *value)
{
    if (**at < '0' || **at > '9' || (**at == '0' && (*at)[1] >= '0' && (*at)[1] <= '9')) {
        return false;
    }
    uint64_t result = 0;
    while (**at >= '0' && **at <= '9') {
        result = result * 10U + (uint64_t)(**at - '0');
        if (result > UINT32_MAX) {
            return false;
        }
        ++*at;
    }
    *value = (uint32_t)result;
    return true;
}

static bool find_field(const char *name, field_t *field)
{
    for (field_t i = 0; i < FIELD_COUNT; ++i) {
        if (strcmp(name, FIELD_NAMES[i]) == 0) {
            *field = i;
            return true;
        }
    }
    return false;
}

static bool parse_member(char **at, provision_request_t *request, uint32_t *version, const char **op,
                         unsigned *seen)
{
    char *name;
    field_t field;
    if (!parse_string(at, &name) || !find_field(name, &field) || (*seen & FIELD_BIT(field)) != 0) {
        return false;
    }
    *seen |= FIELD_BIT(field);
    skip_space(at);
    if (**at != ':') {
        return false;
    }
    ++*at;
    skip_space(at);
    if (field == FIELD_VERSION) {
        return parse_unsigned(at, version);
    }
    if (field == FIELD_SCREEN_TIMEOUT) {
        return parse_unsigned(at, &request->screen_timeout_s);
    }
    char *text;
    if (!parse_string(at, &text)) {
        return false;
    }
    switch (field) {
    case FIELD_OP:
        *op = text;
        break;
    case FIELD_WIFI_SSID:
        request->wifi_ssid = text;
        break;
    case FIELD_WIFI_PASSWORD:
        request->wifi_password = text;
        break;
    case FIELD_BRIDGE_URL:
        request->bridge_url = text;
        break;
    case FIELD_DEVICE_TOKEN:
        request->device_token = text;
        break;
    case FIELD_VERSION:
    case FIELD_SCREEN_TIMEOUT:
    case FIELD_COUNT:
        return false;
    }
    return true;
}

static bool parse_object(char *at, provision_request_t *request, uint32_t *version, const char **op,
                         unsigned *seen)
{
    skip_space(&at);
    if (*at++ != '{') {
        return false;
    }
    skip_space(&at);
    if (*at != '}') {
        for (;;) {
            if (!parse_member(&at, request, version, op, seen)) {
                return false;
            }
            skip_space(&at);
            if (*at != ',') {
                break;
            }
            ++at;
            skip_space(&at);
        }
    }
    if (*at++ != '}') {
        return false;
    }
    skip_space(&at);
    return *at == '\0';
}

bool provision_request_parse(char *line, provision_request_t *request)
{
    memset(request, 0, sizeof(*request));
    uint32_t version = 0;
    const char *op = NULL;
    unsigned seen = 0;
    if (!parse_object(line, request, &version, &op, &seen) || version != PROVISION_PROTOCOL_VERSION ||
        op == NULL) {
        return false;
    }
    if (strcmp(op, OP_CONFIGURE_BRIDGE) == 0 && seen == BRIDGE_FIELDS) {
        request->op = PROVISION_OP_CONFIGURE_BRIDGE;
        return true;
    }
    if (strcmp(op, OP_CONFIGURE) == 0 && seen == CONFIGURE_FIELDS) {
        request->op = PROVISION_OP_CONFIGURE;
        return true;
    }
    return false;
}
