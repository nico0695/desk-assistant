#include "wifi_list.h"

#include <string.h>

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

static bool hex_digit(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

bool wifi_ssid_valid(const char *ssid)
{
    size_t length;
    return bounded_length(ssid, WIFI_SSID_SIZE, &length) && length > 0;
}

bool wifi_password_valid(const char *password)
{
    size_t length;
    if (!bounded_length(password, WIFI_PASSWORD_SIZE, &length)) {
        return false;
    }
    if (length == 0) {
        return true;
    }
    if (length == WIFI_PASSWORD_HEX_KEY_BYTES) {
        for (size_t i = 0; i < length; ++i) {
            if (!hex_digit(password[i])) {
                return false;
            }
        }
        return true;
    }
    if (length < WIFI_PASSWORD_MIN_BYTES || length > WIFI_PASSWORD_MAX_TEXT_BYTES) {
        return false;
    }
    for (size_t i = 0; i < length; ++i) {
        if (password[i] < ' ' || password[i] > '~') {
            return false;
        }
    }
    return true;
}

static bool network_valid(const wifi_network_t *network)
{
    return wifi_ssid_valid(network->ssid) && wifi_password_valid(network->password);
}

bool wifi_list_valid(const wifi_list_t *list)
{
    static const wifi_network_t empty;
    if (list->count > WIFI_LIST_CAPACITY) {
        return false;
    }
    for (unsigned i = 0; i < WIFI_LIST_CAPACITY; ++i) {
        const wifi_network_t *network = &list->networks[i];
        if (i >= list->count) {
            if (memcmp(network, &empty, sizeof(empty)) != 0) {
                return false;
            }
            continue;
        }
        if (!network_valid(network)) {
            return false;
        }
        for (unsigned j = 0; j < i; ++j) {
            if (strcmp(list->networks[j].ssid, network->ssid) == 0) {
                return false;
            }
        }
    }
    return true;
}

int wifi_list_find(const wifi_list_t *list, const char *ssid)
{
    for (unsigned i = 0; i < list->count && i < WIFI_LIST_CAPACITY; ++i) {
        if (strcmp(list->networks[i].ssid, ssid) == 0) {
            return (int)i;
        }
    }
    return -1;
}

wifi_list_save_t wifi_list_save(wifi_list_t *list, const char *ssid, const char *password,
                                char replaced_ssid[WIFI_SSID_SIZE])
{
    if (!wifi_ssid_valid(ssid) || !wifi_password_valid(password) || list->count > WIFI_LIST_CAPACITY) {
        return WIFI_LIST_SAVE_INVALID;
    }
    const int found = wifi_list_find(list, ssid);
    wifi_list_save_t result;
    unsigned shifted;
    if (found >= 0) {
        result = WIFI_LIST_SAVE_UPDATED;
        shifted = (unsigned)found;
    } else if (list->count < WIFI_LIST_CAPACITY) {
        result = WIFI_LIST_SAVE_ADDED;
        shifted = list->count++;
    } else {
        result = WIFI_LIST_SAVE_REPLACED_OLDEST;
        shifted = WIFI_LIST_CAPACITY - 1U;
        if (replaced_ssid != NULL) {
            memcpy(replaced_ssid, list->networks[shifted].ssid, WIFI_SSID_SIZE);
        }
    }
    memmove(&list->networks[1], &list->networks[0], shifted * sizeof(wifi_network_t));
    wifi_network_t *first = &list->networks[0];
    memset(first, 0, sizeof(*first));
    memcpy(first->ssid, ssid, strlen(ssid));
    memcpy(first->password, password, strlen(password));
    return result;
}

bool wifi_list_forget(wifi_list_t *list, const char *ssid)
{
    const int found = wifi_list_find(list, ssid);
    if (found < 0) {
        return false;
    }
    const unsigned index = (unsigned)found;
    memmove(&list->networks[index], &list->networks[index + 1U],
            (list->count - 1U - index) * sizeof(wifi_network_t));
    --list->count;
    memset(&list->networks[list->count], 0, sizeof(wifi_network_t));
    return true;
}
