#include "bridge_http_common.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

bool bridge_http_make_url(const char *base, const char *path, char *url, size_t capacity)
{
    static const char route_prefix[] = "/v1/device/";
    if (!bridge_url_valid(base) || path == NULL || url == NULL || capacity == 0U ||
        strncmp(path, route_prefix, sizeof(route_prefix) - 1U) != 0) {
        return false;
    }
    const char *end = memchr(path, '\0', BRIDGE_HTTP_ROUTE_MAX_BYTES + 1U);
    if (end == NULL) {
        return false;
    }
    for (const char *at = path; at != end; ++at) {
        if (*at <= ' ' || *at > '~' || *at == '#' || *at == '\\' || *at == '.' || *at == '%') {
            return false;
        }
    }
    size_t base_length = strlen(base);
    while (base_length > 0U && base[base_length - 1U] == '/') {
        --base_length;
    }
    int length = snprintf(url, capacity, "%.*s%s", (int)base_length, base, path);
    return length >= 0 && (size_t)length < capacity;
}

int bridge_http_remaining_ms(int64_t deadline_us, int64_t now_us, int requested_ms)
{
    if (deadline_us <= now_us) {
        return 0;
    }
    int64_t remaining = (deadline_us - now_us) / 1000;
    if (remaining > INT_MAX) {
        remaining = INT_MAX;
    }
    if (requested_ms >= 0 && requested_ms < remaining) {
        return requested_ms;
    }
    return (int)remaining;
}
