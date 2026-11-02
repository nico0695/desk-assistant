#pragma once

#include <stdbool.h>

#include "bridge_resources.h"
#include "cJSON.h"

/* Error codes that differ per action; NULL when the action has none. */
typedef struct {
    /* 404: the resource is absent or inaccessible. */
    const char *not_found;
    /* 409: its state or time changed. */
    const char *changed;
    /* The bridge reads before writing, so its 502 read failures mean nothing was sent. */
    bool reads_first;
} bridge_action_codes_t;

/* An action error envelope decided by the HTTP/code pair. Anything unknown after dispatch may have been applied. */
bridge_action_result_t bridge_action_error(int status, const cJSON *root, const bridge_action_codes_t *codes);
