/* The timestamp validation is extracted from event_codec.c in the author's V4 firmware; see THIRD_PARTY_NOTICES.md. */
#include "bridge_time.h"

#include <stdio.h>
#include <string.h>

static unsigned decimal(const char *text, size_t count)
{
    unsigned value = 0;
    for (size_t i = 0; i < count; ++i) value = value * 10U + (unsigned)(text[i] - '0');
    return value;
}

static unsigned days_in_month(unsigned year, unsigned month)
{
    static const unsigned days[] = {31U, 28U, 31U, 30U, 31U, 30U, 31U, 31U, 30U, 31U, 30U, 31U};
    if (month == 2U && year % 4U == 0U && (year % 100U != 0U || year % 400U == 0U)) return 29U;
    return days[month - 1U];
}

bool bridge_time_valid(const char *text)
{
    const size_t length = strnlen(text, BRIDGE_TIME_UTC_SIZE);
    if (length < 20U || length > 30U || text[4] != '-' || text[7] != '-' || text[10] != 'T' ||
        text[13] != ':' || text[16] != ':' || text[length - 1U] != 'Z') return false;
    for (size_t i = 0; i < length - 1U; ++i) {
        if (i == 4U || i == 7U || i == 10U || i == 13U || i == 16U) continue;
        if (i == 19U && length != 20U) {
            if (text[i] != '.' || length < 22U) return false;
            continue;
        }
        if (text[i] < '0' || text[i] > '9') return false;
    }
    const unsigned year = decimal(text, 4U), month = decimal(text + 5U, 2U), day = decimal(text + 8U, 2U);
    if (month < 1U || month > 12U || day < 1U) return false;
    return day <= days_in_month(year, month) && decimal(text + 11U, 2U) < 24U && decimal(text + 14U, 2U) < 60U &&
           decimal(text + 17U, 2U) < 60U;
}

bool bridge_time_format_local(const char *utc, char *text, size_t capacity)
{
    if (text == NULL || capacity == 0U) return false;
    text[0] = '\0';
    if (utc == NULL || !bridge_time_valid(utc)) return false;
    unsigned year = decimal(utc, 4U), month = decimal(utc + 5U, 2U), day = decimal(utc + 8U, 2U);
    unsigned hour = decimal(utc + 11U, 2U);
    const unsigned minute = decimal(utc + 14U, 2U);
    const char *label = "UTC";
    if (year >= BRIDGE_TIME_LOCAL_FIRST_YEAR) {
        label = BRIDGE_TIME_LOCAL_ZONE_LABEL;
        if (hour < BRIDGE_TIME_LOCAL_UTC_OFFSET_HOURS) {
            hour += 24U - BRIDGE_TIME_LOCAL_UTC_OFFSET_HOURS;
            if (--day == 0U) {
                if (--month == 0U) { month = 12U; --year; }
                day = days_in_month(year, month);
            }
        } else hour -= BRIDGE_TIME_LOCAL_UTC_OFFSET_HOURS;
    }
    const int written = snprintf(text, capacity, "%04u-%02u-%02u %02u:%02u %s", year, month, day, hour, minute, label);
    if (written < 0 || (size_t)written >= capacity) {
        text[0] = '\0';
        return false;
    }
    return true;
}
