#pragma once

#include <stdbool.h>
#include <stdint.h>

#define BRIDGE_UUID_LENGTH 36U
#define BRIDGE_UUID_SIZE (BRIDGE_UUID_LENGTH + 1U)

/* A canonical UUID uses lowercase hexadecimal and four fixed hyphens. */
bool bridge_uuid_valid(const char *uuid);

/* Formats 16 random bytes as a canonical version 4 UUID, setting the version and variant bits. */
void bridge_uuid_format(const uint8_t random_bytes[16], char uuid[BRIDGE_UUID_SIZE]);
