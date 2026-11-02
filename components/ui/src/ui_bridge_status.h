#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint64_t generation;
    bool online;
} ui_bridge_status_t;

/* Returns true only if the displayed value changes. */
bool ui_bridge_status_update(ui_bridge_status_t *status, bool online, uint64_t generation);
