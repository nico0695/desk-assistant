#include "bridge_uuid.h"

#include <stddef.h>

bool bridge_uuid_valid(const char *uuid)
{
    if (uuid == NULL) {
        return false;
    }
    for (size_t i = 0; i < BRIDGE_UUID_LENGTH; ++i) {
        const char c = uuid[i];
        if (c == '\0') {
            return false;
        }
        if (i == 8U || i == 13U || i == 18U || i == 23U) {
            if (c != '-') {
                return false;
            }
        } else if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
            return false;
        }
    }
    return uuid[BRIDGE_UUID_LENGTH] == '\0';
}

#define UUID_BYTES 16U
#define UUID_VERSION_BYTE 6U
#define UUID_VARIANT_BYTE 8U

void bridge_uuid_format(const uint8_t random_bytes[16], char uuid[BRIDGE_UUID_SIZE])
{
    static const char hex[] = "0123456789abcdef";
    size_t out = 0;
    for (size_t i = 0; i < UUID_BYTES; ++i) {
        uint8_t value = random_bytes[i];
        if (i == UUID_VERSION_BYTE) {
            value = (uint8_t)((value & 0x0FU) | 0x40U);
        } else if (i == UUID_VARIANT_BYTE) {
            value = (uint8_t)((value & 0x3FU) | 0x80U);
        }
        if (i == 4U || i == 6U || i == 8U || i == 10U) {
            uuid[out++] = '-';
        }
        uuid[out++] = hex[value >> 4];
        uuid[out++] = hex[value & 0x0FU];
    }
    uuid[out] = '\0';
}
