#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "connectivity_contract.h"

typedef enum {
    SETUP_FORM_OK,
    SETUP_FORM_MALFORMED,
    SETUP_FORM_STALE_NONCE,
} setup_form_result_t;

/* Strings point into the body and live as long as it does. Values other than the nonce are not validated here. */
typedef struct {
    const char *ssid;
    const char *password;
} setup_form_t;

/*
 * Decodes an application/x-www-form-urlencoded body of at most SETUP_FORM_MAX_BYTES in place. The buffer must hold
 * length + 1 bytes: the last value is terminated at body[length]. Requires exactly
 * the fields nonce, ssid and password, each once, without NUL; then compares the nonce with the session nonce.
 */
setup_form_result_t setup_form_parse(char *body, size_t length, const char *session_nonce, setup_form_t *form);

/*
 * True when the request reached the access point address from its subnet. Addresses are IPv4 in network byte order;
 * the HTTP server reports them from IPv6 sockets as IPv4-mapped addresses.
 */
bool setup_peer_allowed(uint32_t local_ipv4, uint32_t peer_ipv4, uint32_t ap_ipv4, uint32_t ap_netmask);
