#include "bridge_text.h"

#include <string.h>

#define UTF8_MAX_CODEPOINT 0x10FFFFU
#define UTF8_SURROGATE_FIRST 0xD800U
#define UTF8_SURROGATE_LAST 0xDFFFU

static bool start_sequence(bridge_utf8_t *decoder, unsigned char byte)
{
    if (byte >= 0xC2U && byte <= 0xDFU) {
        *decoder = (bridge_utf8_t){.remaining = 1, .value = byte & 0x1FU, .minimum = 0x80U};
    } else if (byte >= 0xE0U && byte <= 0xEFU) {
        *decoder = (bridge_utf8_t){.remaining = 2, .value = byte & 0x0FU, .minimum = 0x800U};
    } else if (byte >= 0xF0U && byte <= 0xF4U) {
        *decoder = (bridge_utf8_t){.remaining = 3, .value = byte & 0x07U, .minimum = 0x10000U};
    } else {
        return false;
    }
    return true;
}

bool bridge_utf8_step(bridge_utf8_t *decoder, unsigned char byte)
{
    if (decoder->remaining == 0U) {
        return byte < 0x80U || start_sequence(decoder, byte);
    }
    if ((byte & 0xC0U) != 0x80U) {
        return false;
    }
    decoder->value = (decoder->value << 6U) | (uint32_t)(byte & 0x3FU);
    if (--decoder->remaining != 0U) {
        return true;
    }
    return decoder->value >= decoder->minimum && decoder->value <= UTF8_MAX_CODEPOINT &&
           (decoder->value < UTF8_SURROGATE_FIRST || decoder->value > UTF8_SURROGATE_LAST);
}

bool bridge_utf8_valid(const char *text, size_t length)
{
    bridge_utf8_t decoder = {0};
    for (size_t i = 0; i < length; ++i) {
        if (!bridge_utf8_step(&decoder, (unsigned char)text[i])) {
            return false;
        }
    }
    return decoder.remaining == 0U;
}

bool bridge_json_bounded(const char *data, size_t length, unsigned max_objects, unsigned max_members)
{
    bool quoted = false;
    unsigned objects = 0;
    unsigned members = 0;
    for (size_t i = 0; i < length; ++i) {
        const unsigned char c = (unsigned char)data[i];
        if (c == 0U) {
            return false;
        }
        if (quoted) {
            if (c < 0x20U) {
                return false;
            }
            if (c == '\\') {
                if (++i == length) {
                    return false;
                }
                if (data[i] == 'u' && length - i >= 5U && memcmp(data + i + 1U, "0000", 4U) == 0) {
                    return false;
                }
            } else if (c == '"') {
                quoted = false;
            }
        } else if (c == '"') {
            quoted = true;
        } else if (c == '{') {
            if (++objects > max_objects) {
                return false;
            }
        } else if (c == '[' || c == ']') {
            return false;
        } else if (c == ':') {
            if (++members > max_members) {
                return false;
            }
        } else if (c < 0x20U && c != '\t' && c != '\r' && c != '\n') {
            return false;
        }
    }
    return objects > 0U && !quoted;
}
