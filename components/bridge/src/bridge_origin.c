#include "bridge_origin.h"

#include <stddef.h>
#include <string.h>

#define URL_SCHEME "https://"
#define DNS_NAME_MAX_BYTES 253U
#define DNS_LABEL_MAX_BYTES 63U
#define IPV4_OCTET_COUNT 4U
#define IPV4_OCTET_MAX 255U
#define IPV4_OCTET_MAX_DIGITS 3U
#define PORT_MAX 65535U

static bool bounded_length(const char *text, size_t capacity, size_t *length)
{
    if (text == NULL) {
        return false;
    }
    const char *end = memchr(text, '\0', capacity);
    if (end == NULL) {
        return false;
    }
    *length = (size_t)(end - text);
    return true;
}

static bool is_digit(char c)
{
    return c >= '0' && c <= '9';
}

static bool is_hex(char c)
{
    return is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static bool is_label_char(char c)
{
    return is_digit(c) || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '-';
}

/* Four decimal octets without leading zeros. */
static bool ipv4_valid(const char *host, size_t length)
{
    unsigned octets = 0;
    size_t i = 0;
    while (i <= length) {
        const size_t start = i;
        unsigned value = 0;
        while (i < length && host[i] != '.') {
            if (!is_digit(host[i])) {
                return false;
            }
            value = value * 10U + (unsigned)(host[i] - '0');
            if (i - start >= IPV4_OCTET_MAX_DIGITS) {
                return false;
            }
            ++i;
        }
        const size_t digits = i - start;
        if (digits == 0 || value > IPV4_OCTET_MAX || (digits > 1 && host[start] == '0')) {
            return false;
        }
        ++octets;
        ++i;
    }
    return octets == IPV4_OCTET_COUNT;
}

static bool all_digits_and_dots(const char *host, size_t length)
{
    for (size_t i = 0; i < length; ++i) {
        if (!is_digit(host[i]) && host[i] != '.') {
            return false;
        }
    }
    return true;
}

static bool dns_name_valid(const char *host, size_t length)
{
    if (length > 0 && host[length - 1] == '.') {
        --length;
    }
    if (length == 0 || length > DNS_NAME_MAX_BYTES) {
        return false;
    }
    size_t i = 0;
    while (i <= length) {
        const size_t start = i;
        while (i < length && host[i] != '.') {
            if (!is_label_char(host[i])) {
                return false;
            }
            ++i;
        }
        const size_t label = i - start;
        if (label == 0 || label > DNS_LABEL_MAX_BYTES || host[start] == '-' || host[i - 1] == '-') {
            return false;
        }
        ++i;
    }
    return true;
}

static bool host_valid(const char *host, size_t length)
{
    if (length > 0 && all_digits_and_dots(host, length)) {
        return ipv4_valid(host, length);
    }
    return dns_name_valid(host, length);
}

static bool port_valid(const char *port, size_t length)
{
    if (length == 0) {
        return false;
    }
    unsigned long value = 0;
    for (size_t i = 0; i < length; ++i) {
        if (!is_digit(port[i])) {
            return false;
        }
        value = value * 10U + (unsigned long)(port[i] - '0');
        if (value > PORT_MAX) {
            return false;
        }
    }
    return value > 0;
}

/* Printable ASCII without spaces, userinfo, query, fragment or backslash; percent escapes are complete. */
static bool url_characters_valid(const char *text, size_t length)
{
    for (size_t i = 0; i < length; ++i) {
        const char c = text[i];
        if (c <= ' ' || c > '~' || c == '@' || c == '?' || c == '#' || c == '\\') {
            return false;
        }
        if (c == '%') {
            if (i + 2 >= length || !is_hex(text[i + 1]) || !is_hex(text[i + 2])) {
                return false;
            }
            i += 2;
        }
    }
    return true;
}

bool bridge_url_valid(const char *url)
{
    size_t length;
    const size_t scheme_length = sizeof(URL_SCHEME) - 1U;
    if (!bounded_length(url, BRIDGE_URL_SIZE, &length) || length <= scheme_length ||
        strncmp(url, URL_SCHEME, scheme_length) != 0) {
        return false;
    }
    const char *authority = url + scheme_length;
    const size_t rest = length - scheme_length;
    if (!url_characters_valid(authority, rest)) {
        return false;
    }
    const size_t authority_length = strcspn(authority, "/");
    const char *colon = memchr(authority, ':', authority_length);
    const size_t host_length = colon != NULL ? (size_t)(colon - authority) : authority_length;
    if (!host_valid(authority, host_length)) {
        return false;
    }
    return colon == NULL || port_valid(colon + 1, authority_length - host_length - 1U);
}

bool bridge_token_valid(const char *token)
{
    size_t length;
    if (!bounded_length(token, BRIDGE_TOKEN_SIZE, &length) || length < BRIDGE_TOKEN_MIN_BYTES) {
        return false;
    }
    for (size_t i = 0; i < length; ++i) {
        if (token[i] <= ' ' || token[i] > '~') {
            return false;
        }
    }
    return true;
}
