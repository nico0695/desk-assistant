#pragma once

#include <stdbool.h>

#define BRIDGE_UUID_LENGTH 36U
#define BRIDGE_UUID_SIZE (BRIDGE_UUID_LENGTH + 1U)

/* A canonical UUID uses lowercase hexadecimal and four fixed hyphens. */
bool bridge_uuid_valid(const char *uuid);
