#pragma once

#include <stddef.h>
#include <stdint.h>

#include "bridge_http.h"
#include "bridge_snooze.h"

/*
 * The one Snooze POST for the alert time the user saw; the response lands in body. Arguments that cannot form a
 * request are INVALID_ARGUMENT and unsent. Never retried.
 */
bridge_http_mutation_t bridge_snooze_send(const bridge_origin_t *origin, uint64_t alert_id,
                                          bridge_snooze_preset_t preset, const char *scheduled_at, char *body,
                                          size_t capacity);
