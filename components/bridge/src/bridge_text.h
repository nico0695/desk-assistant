#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Incremental UTF-8 validation: shortest forms only, no surrogates, nothing above U+10FFFF. */
typedef struct {
    uint8_t remaining;
    uint32_t value;
    uint32_t minimum;
} bridge_utf8_t;

/* False when byte cannot continue valid UTF-8. A sequence is complete when remaining is zero. */
bool bridge_utf8_step(bridge_utf8_t *decoder, unsigned char byte);
bool bridge_utf8_valid(const char *text, size_t length);

/*
 * Resource guard before cJSON allocates: at most max_objects objects and max_members members, no arrays, no raw
 * control characters inside strings and no escaped NUL, which cJSON would hide by returning strings without length.
 * It does not validate JSON; cJSON still parses it.
 */
bool bridge_json_bounded(const char *data, size_t length, unsigned max_objects, unsigned max_members);
