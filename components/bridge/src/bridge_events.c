/* Event response validation adapted from event_codec.c in the author's V4 firmware; see THIRD_PARTY_NOTICES.md. */
#include "bridge_events.h"
#include "bridge_json.h"
#include "bridge_text.h"
#include "bridge_time.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "bridge_uuid.h"

bool bridge_event_valid(const bridge_event_t *event)
{
    if (event == NULL) return false;
    const size_t length = strnlen(event->message, sizeof(event->message));
    return bridge_uuid_valid(event->event_id) && event->alert_id > 0U &&
           event->alert_id <= UINT64_C(9007199254740991) && length <= BRIDGE_EVENT_MESSAGE_MAX_BYTES &&
           bridge_utf8_valid(event->message, length) && bridge_time_valid(event->scheduled_at) &&
           bridge_time_valid(event->triggered_at);
}

static bool copy_string(const cJSON *item, char *target, size_t capacity)
{
    if (!cJSON_IsString(item) || !item->valuestring || strlen(item->valuestring) >= capacity)
        return false;
    memcpy(target, item->valuestring, strlen(item->valuestring) + 1);
    return true;
}

bridge_event_decode_t bridge_event_decode(const char *body, size_t length, int http_status,
                                                  bridge_event_t *event, bool *present)
{
    if (!body || !event || !present || http_status != 200 || !length) return BRIDGE_EVENT_DECODE_INVALID;
    if (length > BRIDGE_EVENT_RESPONSE_MAX_BYTES) return BRIDGE_EVENT_DECODE_TOO_LARGE;
    if (memchr(body, '\0', length) || !bridge_json_guard(body, length, BRIDGE_JSON_MAX_DEPTH)) return BRIDGE_EVENT_DECODE_INVALID;
    char *terminated = malloc(length + 1);
    if (!terminated) return BRIDGE_EVENT_DECODE_INVALID;
    memcpy(terminated, body, length);
    terminated[length] = '\0';
    if (!bridge_utf8_valid(terminated, length)) {
        free(terminated);
        return BRIDGE_EVENT_DECODE_INVALID;
    }
    cJSON *root = cJSON_ParseWithLengthOpts(terminated, length + 1, NULL, true);
    free(terminated);
    if (!root) return BRIDGE_EVENT_DECODE_INVALID;
    const char *const wrapper_fields[] = {"events"};
    const char *const event_fields[] = {"event_id", "type", "alert_id", "message", "scheduled_at", "triggered_at"};
    const cJSON *events = cJSON_GetObjectItemCaseSensitive(root, "events");
    bool valid = bridge_json_fields_exact(root, wrapper_fields, 1) && cJSON_IsArray(events) &&
                 cJSON_GetArraySize(events) <= 1;
    bool decoded_present = valid && cJSON_GetArraySize(events) == 1;
    bridge_event_t decoded = {0};
    bool too_large = false;
    if (decoded_present) {
        const cJSON *item = cJSON_GetArrayItem(events, 0);
        const cJSON *alert = cJSON_GetObjectItemCaseSensitive(item, "alert_id");
        valid = bridge_json_fields_exact(item, event_fields, 6) && cJSON_IsNumber(alert) &&
                isfinite(alert->valuedouble) && alert->valuedouble >= 1 &&
                alert->valuedouble <= 9007199254740991.0 &&
                floor(alert->valuedouble) == alert->valuedouble;
        const cJSON *message = cJSON_GetObjectItemCaseSensitive(item, "message");
        if (valid && cJSON_IsString(message) && message->valuestring &&
            strlen(message->valuestring) >= BRIDGE_EVENT_MESSAGE_SIZE) {
            too_large = true;
            valid = false;
        }
        if (valid) {
            valid = cJSON_IsString(cJSON_GetObjectItemCaseSensitive(item, "type")) &&
                    strcmp(cJSON_GetObjectItemCaseSensitive(item, "type")->valuestring, "alert.triggered") == 0;
        }
        if (valid) {
            decoded.alert_id = (uint64_t)alert->valuedouble;
            valid = copy_string(cJSON_GetObjectItemCaseSensitive(item, "event_id"),
                                decoded.event_id, sizeof(decoded.event_id)) &&
                    copy_string(cJSON_GetObjectItemCaseSensitive(item, "message"),
                                decoded.message, sizeof(decoded.message)) &&
                    copy_string(cJSON_GetObjectItemCaseSensitive(item, "scheduled_at"),
                                decoded.scheduled_at, sizeof(decoded.scheduled_at)) &&
                    copy_string(cJSON_GetObjectItemCaseSensitive(item, "triggered_at"),
                                decoded.triggered_at, sizeof(decoded.triggered_at)) &&
                    bridge_event_valid(&decoded);
        }
    }
    if (valid) {
        *event = decoded;
        *present = decoded_present;
    }
    cJSON_Delete(root);
    return valid ? BRIDGE_EVENT_DECODE_OK : too_large ? BRIDGE_EVENT_DECODE_TOO_LARGE : BRIDGE_EVENT_DECODE_INVALID;
}

bool bridge_event_ack_succeeded(int http_status, size_t body_length)
{
    return http_status == 204 && body_length == 0;
}

bool bridge_event_ack_rejected(int http_status)
{
    return http_status == 400 || http_status == 404;
}
