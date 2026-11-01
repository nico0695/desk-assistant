#include "setup_form.h"

#include <string.h>

#include "hex_digit.h"

typedef enum {
    FORM_FIELD_NONCE,
    FORM_FIELD_SSID,
    FORM_FIELD_PASSWORD,
    FORM_FIELD_COUNT,
} form_field_t;

static const char *const FORM_FIELD_NAMES[FORM_FIELD_COUNT] = {
    [FORM_FIELD_NONCE] = "nonce",
    [FORM_FIELD_SSID] = "ssid",
    [FORM_FIELD_PASSWORD] = "password",
};

/* Decodes text[0, length) in place and NUL-terminates it; rejects incomplete escapes and NUL. */
static bool decode_component(char *text, size_t length)
{
    size_t out = 0;
    for (size_t i = 0; i < length; ++i) {
        char c = text[i];
        if (c == '+') {
            c = ' ';
        } else if (c == '%') {
            if (i + 2 >= length) {
                return false;
            }
            const int high = hex_digit_value(text[i + 1]);
            const int low = hex_digit_value(text[i + 2]);
            if (high < 0 || low < 0) {
                return false;
            }
            c = (char)((high << 4) | low);
            i += 2;
        }
        if (c == '\0') {
            return false;
        }
        text[out++] = c;
    }
    text[out] = '\0';
    return true;
}

static bool find_form_field(const char *name, form_field_t *field)
{
    for (form_field_t i = 0; i < FORM_FIELD_COUNT; ++i) {
        if (strcmp(name, FORM_FIELD_NAMES[i]) == 0) {
            *field = i;
            return true;
        }
    }
    return false;
}

setup_form_result_t setup_form_parse(char *body, size_t length, const char *session_nonce, setup_form_t *form)
{
    memset(form, 0, sizeof(*form));
    if (length > SETUP_FORM_MAX_BYTES || memchr(body, '\0', length) != NULL) {
        return SETUP_FORM_MALFORMED;
    }
    const char *values[FORM_FIELD_COUNT] = {0};
    size_t start = 0;
    while (start <= length) {
        size_t end = start;
        while (end < length && body[end] != '&') {
            ++end;
        }
        char *pair = body + start;
        const size_t pair_length = end - start;
        char *equals = memchr(pair, '=', pair_length);
        if (equals == NULL) {
            return SETUP_FORM_MALFORMED;
        }
        const size_t name_length = (size_t)(equals - pair);
        /* Both parts are decoded in place; '=', '&' and body[length] become their terminators. */
        if (!decode_component(pair, name_length) ||
            !decode_component(equals + 1, pair_length - name_length - 1U)) {
            return SETUP_FORM_MALFORMED;
        }
        form_field_t field;
        if (!find_form_field(pair, &field) || values[field] != NULL) {
            return SETUP_FORM_MALFORMED;
        }
        values[field] = equals + 1;
        start = end + 1;
    }
    for (form_field_t i = 0; i < FORM_FIELD_COUNT; ++i) {
        if (values[i] == NULL) {
            return SETUP_FORM_MALFORMED;
        }
    }
    if (session_nonce == NULL || session_nonce[0] == '\0' || strcmp(values[FORM_FIELD_NONCE], session_nonce) != 0) {
        return SETUP_FORM_STALE_NONCE;
    }
    form->ssid = values[FORM_FIELD_SSID];
    form->password = values[FORM_FIELD_PASSWORD];
    return SETUP_FORM_OK;
}

bool setup_peer_allowed(uint32_t local_ipv4, uint32_t peer_ipv4, uint32_t ap_ipv4, uint32_t ap_netmask)
{
    return local_ipv4 == ap_ipv4 && (peer_ipv4 & ap_netmask) == (ap_ipv4 & ap_netmask);
}
