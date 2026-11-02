#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "cJSON.h"

/* Deepest nesting any bridge response uses. */
#define BRIDGE_JSON_MAX_DEPTH 4U

/* True when object has exactly the named members, each once, and no others. */
bool bridge_json_fields_exact(const cJSON *object, const char *const *names, size_t count);

/*
 * Resource guard before cJSON allocates: balanced nesting up to max_depth, no raw control characters inside strings,
 * no escaped NUL (cJSON would hide it by returning strings without length) and every number a positive safe integer,
 * so rounding cannot turn a fractional or unsafe value into a valid one. It does not validate JSON; cJSON still parses it.
 */
bool bridge_json_guard(const char *body, size_t length, unsigned max_depth);
