/* Adapted from snooze_codec.c in the author's V4 firmware; see THIRD_PARTY_NOTICES.md. */
#include "bridge_snooze.h"
#include "bridge_time.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bridge_json.h"
#include "bridge_text.h"
#include "cJSON.h"

bool bridge_snooze_path(uint64_t alert_id, char *path, size_t capacity)
{
    if (alert_id == 0U || alert_id > UINT64_C(9007199254740991) || path == NULL || capacity == 0U) return false;
    const int length = snprintf(path, capacity, BRIDGE_SNOOZE_PATH_FORMAT, (unsigned long long)alert_id);
    return length > 0 && (size_t)length < capacity;
}

bool bridge_snooze_request(bridge_snooze_preset_t preset, const char *scheduled_at,
                           char *body, size_t capacity)
{
    static const char *const keys[] = {"snooze_5m", "snooze_1h", "snooze_tomorrow_same_time"};
    if (preset < BRIDGE_SNOOZE_FIVE_MINUTES || preset > BRIDGE_SNOOZE_TOMORROW ||
        scheduled_at == NULL || body == NULL || capacity == 0U ||
        !bridge_time_valid(scheduled_at)) return false;
    const int length = snprintf(body, capacity, "{\"preset\":\"%s\",\"expected_scheduled_at\":\"%s\"}",
                                keys[preset - BRIDGE_SNOOZE_FIVE_MINUTES], scheduled_at);
    return length > 0 && (size_t)length < capacity;
}

bridge_action_result_t bridge_snooze_decode(int status, const char *body, size_t length,
                                            uint64_t alert_id, char confirmed_at[BRIDGE_EVENT_TIME_SIZE])
{
    if (confirmed_at != NULL) confirmed_at[0] = '\0';
    if (body == NULL || confirmed_at == NULL || alert_id == 0U || alert_id > UINT64_C(9007199254740991) ||
        length == 0U || length > BRIDGE_EVENT_RESPONSE_MAX_BYTES ||
        !bridge_json_bounded(body, length, 2U, 3U) || !bridge_utf8_valid(body, length))
        return BRIDGE_ACTION_UNCONFIRMED;
    char *terminated = malloc(length + 1U);
    if (terminated == NULL) return BRIDGE_ACTION_UNCONFIRMED;
    memcpy(terminated, body, length);
    terminated[length] = '\0';
    cJSON *root = cJSON_ParseWithLengthOpts(terminated, length + 1U, NULL, true);
    free(terminated);
    if (root == NULL) return BRIDGE_ACTION_UNCONFIRMED;
    bridge_action_result_t result = BRIDGE_ACTION_UNCONFIRMED;
    if (status == 200) {
        static const char *const names[] = {"alert_id", "scheduled_at"};
        const cJSON *id = cJSON_GetObjectItemCaseSensitive(root, "alert_id");
        const cJSON *time = cJSON_GetObjectItemCaseSensitive(root, "scheduled_at");
        if (bridge_json_fields_exact(root, names, 2U) && cJSON_IsNumber(id) &&
            isfinite(id->valuedouble) && id->valuedouble == (double)alert_id &&
            cJSON_IsString(time) && time->valuestring != NULL &&
            bridge_time_valid(time->valuestring)) {
            memcpy(confirmed_at, time->valuestring, strlen(time->valuestring) + 1U);
            result = BRIDGE_ACTION_SUCCESS;
        }
    } else {
        static const char *const wrapper[] = {"error"};
        static const char *const fields[] = {"code", "message"};
        const cJSON *error = cJSON_GetObjectItemCaseSensitive(root, "error");
        const cJSON *code = cJSON_GetObjectItemCaseSensitive(error, "code");
        const cJSON *message = cJSON_GetObjectItemCaseSensitive(error, "message");
        if (bridge_json_fields_exact(root, wrapper, 1U) && bridge_json_fields_exact(error, fields, 2U) &&
            cJSON_IsString(code) && cJSON_IsString(message) && code->valuestring != NULL) {
            const char *name = code->valuestring;
            if (status == 409 && strcmp(name, "alert_schedule_changed") == 0)
                result = BRIDGE_ACTION_CHANGED;
            else if (status == 404 && strcmp(name, "alert_not_found") == 0)
                result = BRIDGE_ACTION_NOT_FOUND;
            else if (status == 503 && strcmp(name, "temporarily_unavailable") == 0)
                result = BRIDGE_ACTION_NOT_SENT;
            else if ((status == 400 && strcmp(name, "invalid_request") == 0) ||
                     (status == 401 && strcmp(name, "unauthorized") == 0) ||
                     (status == 502 && (strcmp(name, "bot_auth_rejected") == 0 ||
                                        strcmp(name, "bot_access_denied") == 0)))
                result = BRIDGE_ACTION_REJECTED;
        }
    }
    cJSON_Delete(root);
    return result;
}
