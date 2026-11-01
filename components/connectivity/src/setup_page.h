#pragma once

#include <stddef.h>
#include "connectivity_contract.h"

typedef enum {
    SETUP_PAGE_SAVED,
    /* The network was saved in place of the oldest one; its name is shown. */
    SETUP_PAGE_SAVED_REPLACED,
    SETUP_PAGE_INVALID,
    SETUP_PAGE_EXPIRED,
    SETUP_PAGE_FAILED,
} setup_page_result_t;

/* Both pages fit in this many bytes, including the terminator. */
#define SETUP_PAGE_MAX_BYTES 4096U

/*
 * Render a self-contained page into out (capacity bytes). Return its length, or -1 if it does not fit.
 * The form carries the session nonce; the result names the replaced network, escaped, only for SETUP_PAGE_SAVED_REPLACED.
 */
int setup_page_form(char *out, size_t capacity, const char *nonce);
int setup_page_result(char *out, size_t capacity, setup_page_result_t result, const char *replaced_ssid);
