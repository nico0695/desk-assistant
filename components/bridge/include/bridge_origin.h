#pragma once

#include <stdbool.h>

/* The bridge origin: base URL and device token. Both are private configuration. */
#define BRIDGE_URL_MAX_BYTES 256U
#define BRIDGE_URL_SIZE (BRIDGE_URL_MAX_BYTES + 1U)
#define BRIDGE_TOKEN_MIN_BYTES 32U
#define BRIDGE_TOKEN_MAX_BYTES 256U
#define BRIDGE_TOKEN_SIZE (BRIDGE_TOKEN_MAX_BYTES + 1U)

/*
 * Syntax only, no lookup: https:// with a DNS name or dotted IPv4 host, an optional port 1-65535 and an optional
 * path prefix; no userinfo, query, fragment, spaces or backslashes, and valid percent escapes.
 */
bool bridge_url_valid(const char *url);

/* 32-256 printable ASCII characters without spaces. */
bool bridge_token_valid(const char *token);
