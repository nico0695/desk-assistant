#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "bridge_origin.h"

#define BRIDGE_HTTP_ROUTE_MAX_BYTES 160U
#define BRIDGE_HTTP_URL_SIZE (BRIDGE_URL_SIZE + BRIDGE_HTTP_ROUTE_MAX_BYTES)

bool bridge_http_make_url(const char *base, const char *path, char *url, size_t capacity);
int bridge_http_remaining_ms(int64_t deadline_us, int64_t now_us, int requested_ms);
