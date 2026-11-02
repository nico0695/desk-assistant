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
