#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "bridge_origin_store.h"

typedef bool (*bridge_diagnostic_current_fn)(uint64_t ticket);

/* Runs at most once per boot; copies the origin before returning. */
void bridge_diagnostic_start(const bridge_origin_t *origin, uint64_t ticket,
                             bridge_diagnostic_current_fn current);
