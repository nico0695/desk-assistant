#pragma once

#include <stdbool.h>
#include <stddef.h>

/* Complete UTC wire form: up to 30 characters and the terminator. */
#define BRIDGE_TIME_UTC_SIZE 31U
#define BRIDGE_TIME_LOCAL_UTC_OFFSET_HOURS 3U
#define BRIDGE_TIME_LOCAL_ZONE_LABEL "UTC-3"
/* Earlier years keep an explicit UTC label: the server clock was not meaningful then. */
#define BRIDGE_TIME_LOCAL_FIRST_YEAR 2010U
/* "YYYY-MM-DD HH:MM UTC-3" and its terminator. */
#define BRIDGE_TIME_LOCAL_SIZE 24U

/* Exact bridge UTC timestamp: YYYY-MM-DDTHH:MM:SS[.fraction]Z with a real calendar date. */
bool bridge_time_valid(const char *text);

/*
 * Formats a bridge UTC timestamp for display. False, with an empty text, when utc is not a valid bridge timestamp
 * or the buffer is too small. The original UTC value stays the one sent back in actions.
 */
bool bridge_time_format_local(const char *utc, char *text, size_t capacity);
