#include "bridge_resources.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bridge_json.h"
#include "bridge_text.h"

static const char *const ROUTES[BRIDGE_RESOURCE_KIND_COUNT] = {"tasks", "notes", "alerts"};

static const char *const LIST_PAGE_FIELDS[] = {"items", "page", "page_size", "has_previous", "has_next"};
static const char *const LIST_ITEM_FIELDS[BRIDGE_RESOURCE_KIND_COUNT][5] = {
    [BRIDGE_RESOURCE_TASKS] = {"id", "title", "status", "alert_at", "created_at"},
    [BRIDGE_RESOURCE_NOTES] = {"id", "title", "created_at"},
    [BRIDGE_RESOURCE_ALERTS] = {"id", "message", "scheduled_at", "created_at"},
};
static const size_t LIST_ITEM_FIELD_COUNT[BRIDGE_RESOURCE_KIND_COUNT] = {5U, 3U, 4U};
static const char *const DETAIL_FIELDS[BRIDGE_RESOURCE_KIND_COUNT][8] = {
    [BRIDGE_RESOURCE_TASKS] = {"id", "title", "description", "tag", "channel_id", "status", "alert_at", "created_at"},
    [BRIDGE_RESOURCE_NOTES] = {"id", "title", "description", "tag", "channel_id", "created_at", "updated_at"},
    [BRIDGE_RESOURCE_ALERTS] = {"id", "message", "scheduled_at", "sent", "channel_id", "created_at"},
};
static const size_t DETAIL_FIELD_COUNT[BRIDGE_RESOURCE_KIND_COUNT] = {8U, 7U, 6U};

bool bridge_resource_list_path(bridge_resource_kind_t kind, uint64_t page, char *path, size_t capacity)
{
    if (kind >= BRIDGE_RESOURCE_KIND_COUNT || path == NULL || capacity == 0U || page == 0U ||
        (page - 1U) > BRIDGE_RESOURCE_ID_MAX / BRIDGE_RESOURCE_PAGE_SIZE) return false;
    const int length = snprintf(path, capacity, "/v1/device/%s?page=%llu", ROUTES[kind], (unsigned long long)page);
    return length > 0 && (size_t)length < capacity;
}

bool bridge_resource_detail_path(bridge_resource_kind_t kind, uint64_t id, char *path, size_t capacity)
{
    if (kind >= BRIDGE_RESOURCE_KIND_COUNT || path == NULL || capacity == 0U || id == 0U ||
        id > BRIDGE_RESOURCE_ID_MAX) return false;
    const int length = snprintf(path, capacity, "/v1/device/%s/%llu", ROUTES[kind], (unsigned long long)id);
    return length > 0 && (size_t)length < capacity;
}

/* Parses a guarded body. The guard makes every number a positive safe integer. */
static cJSON *parse(const char *body, size_t length)
{
    if (body == NULL || length == 0U || memchr(body, '\0', length) != NULL ||
        !bridge_json_guard(body, length, BRIDGE_JSON_MAX_DEPTH) || !bridge_utf8_valid(body, length)) return NULL;
    char *terminated = malloc(length + 1U);
    if (terminated == NULL) return NULL;
    memcpy(terminated, body, length);
    terminated[length] = '\0';
    cJSON *root = cJSON_ParseWithLengthOpts(terminated, length + 1U, NULL, true);
    free(terminated);
    return root;
}

static bool safe_integer(const cJSON *item, uint64_t *value)
{
    if (!cJSON_IsNumber(item) || !isfinite(item->valuedouble) || item->valuedouble < 1.0 ||
        item->valuedouble > (double)BRIDGE_RESOURCE_ID_MAX || floor(item->valuedouble) != item->valuedouble)
        return false;
    *value = (uint64_t)item->valuedouble;
    return true;
}

static bool wire_time(const cJSON *item, char *target, bool nullable)
{
    if (nullable && cJSON_IsNull(item)) {
        target[0] = '\0';
        return true;
    }
    if (!cJSON_IsString(item) || item->valuestring == NULL || !bridge_time_valid(item->valuestring)) return false;
    memcpy(target, item->valuestring, strlen(item->valuestring) + 1U);
    return true;
}

typedef struct {
    char *next;
    size_t remaining;
    bool exhausted;
} arena_t;

/* Copies a complete string; NULL, and the arena marked exhausted, when it does not fit. */
static const char *arena_copy(arena_t *arena, const cJSON *item)
{
    if (!cJSON_IsString(item) || item->valuestring == NULL) return NULL;
    const size_t size = strlen(item->valuestring) + 1U;
    if (size > arena->remaining) {
        arena->exhausted = true;
        return NULL;
    }
    char *copy = arena->next;
    memcpy(copy, item->valuestring, size);
    arena->next += size;
    arena->remaining -= size;
    return copy;
}

static bridge_resource_result_t error_result(int status, const char *body, size_t length)
{
    cJSON *root = parse(body, length);
    if (root == NULL) return BRIDGE_RESOURCE_INVALID;
    static const char *const wrapper[] = {"error"};
    static const char *const fields[] = {"code", "message"};
    const cJSON *error = cJSON_GetObjectItemCaseSensitive(root, "error");
    const cJSON *code = cJSON_GetObjectItemCaseSensitive(error, "code");
    const cJSON *message = cJSON_GetObjectItemCaseSensitive(error, "message");
    bridge_resource_result_t result = BRIDGE_RESOURCE_INVALID;
    if (bridge_json_fields_exact(root, wrapper, 1U) && bridge_json_fields_exact(error, fields, 2U) &&
        cJSON_IsString(code) && code->valuestring != NULL && cJSON_IsString(message)) {
        const char *name = code->valuestring;
        result = BRIDGE_RESOURCE_REJECTED;
        if (status == 401 && strcmp(name, "unauthorized") == 0) result = BRIDGE_RESOURCE_UNAUTHORIZED;
        else if (status == 404 && strcmp(name, "resource_not_found") == 0) result = BRIDGE_RESOURCE_NOT_FOUND;
        else if (status == 502 && (strcmp(name, "bot_auth_rejected") == 0 || strcmp(name, "bot_access_denied") == 0))
            result = BRIDGE_RESOURCE_DENIED;
        else if ((status == 502 && (strcmp(name, "upstream_unavailable") == 0 ||
                                    strcmp(name, "invalid_upstream_response") == 0)) ||
                 (status == 503 && strcmp(name, "temporarily_unavailable") == 0) ||
                 (status == 500 && strcmp(name, "internal_error") == 0))
            result = BRIDGE_RESOURCE_UNAVAILABLE;
    }
    cJSON_Delete(root);
    return result;
}

static bool optional_text(const cJSON *item)
{
    return cJSON_IsNull(item) || (cJSON_IsString(item) && item->valuestring != NULL);
}

static bool list_entry(bridge_resource_kind_t kind, const cJSON *item, arena_t *arena, bridge_resource_entry_t *entry)
{
    if (!bridge_json_fields_exact(item, LIST_ITEM_FIELDS[kind], LIST_ITEM_FIELD_COUNT[kind])) return false;
    const char *const text_name = kind == BRIDGE_RESOURCE_ALERTS ? "message" : "title";
    memset(entry, 0, sizeof(*entry));
    entry->text = arena_copy(arena, cJSON_GetObjectItemCaseSensitive(item, text_name));
    if (!safe_integer(cJSON_GetObjectItemCaseSensitive(item, "id"), &entry->id) || entry->text == NULL ||
        !wire_time(cJSON_GetObjectItemCaseSensitive(item, "created_at"), entry->created_at, false)) return false;
    if (kind == BRIDGE_RESOURCE_TASKS) {
        const cJSON *status = cJSON_GetObjectItemCaseSensitive(item, "status");
        return cJSON_IsString(status) && status->valuestring != NULL && strcmp(status->valuestring, "pending") == 0 &&
               wire_time(cJSON_GetObjectItemCaseSensitive(item, "alert_at"), entry->when_at, true);
    }
    if (kind == BRIDGE_RESOURCE_ALERTS)
        return wire_time(cJSON_GetObjectItemCaseSensitive(item, "scheduled_at"), entry->when_at, false);
    return true;
}

bridge_resource_result_t bridge_resource_decode_page(bridge_resource_kind_t kind, uint64_t page, int status,
                                                     const char *body, size_t length, char *arena_memory,
                                                     size_t arena_capacity, bridge_resource_page_t *result)
{
    if (kind >= BRIDGE_RESOURCE_KIND_COUNT || page == 0U || page > BRIDGE_RESOURCE_ID_MAX || body == NULL ||
        arena_memory == NULL || result == NULL) return BRIDGE_RESOURCE_INVALID;
    if (length > BRIDGE_RESOURCE_BODY_MAX_BYTES) return BRIDGE_RESOURCE_TOO_LARGE;
    if (status != 200) return error_result(status, body, length);
    cJSON *root = parse(body, length);
    if (root == NULL) return BRIDGE_RESOURCE_INVALID;

    arena_t arena = {.next = arena_memory, .remaining = arena_capacity};
    bridge_resource_page_t decoded = {.kind = kind, .page = page};
    const cJSON *items = cJSON_GetObjectItemCaseSensitive(root, "items");
    const cJSON *reported_page = cJSON_GetObjectItemCaseSensitive(root, "page");
    const cJSON *size = cJSON_GetObjectItemCaseSensitive(root, "page_size");
    const cJSON *previous = cJSON_GetObjectItemCaseSensitive(root, "has_previous");
    const cJSON *next = cJSON_GetObjectItemCaseSensitive(root, "has_next");
    uint64_t echoed = 0U, page_size = 0U;
    bool valid = bridge_json_fields_exact(root, LIST_PAGE_FIELDS, 5U) && cJSON_IsArray(items) &&
                 safe_integer(reported_page, &echoed) && echoed == page && safe_integer(size, &page_size) &&
                 page_size == BRIDGE_RESOURCE_PAGE_SIZE && cJSON_IsBool(previous) && cJSON_IsBool(next) &&
                 cJSON_IsTrue(previous) == (page > 1U) && cJSON_GetArraySize(items) <= (int)BRIDGE_RESOURCE_PAGE_SIZE;
    if (valid) {
        decoded.has_previous = cJSON_IsTrue(previous);
        decoded.has_next = cJSON_IsTrue(next);
        decoded.count = (uint8_t)cJSON_GetArraySize(items);
        valid = !decoded.has_next || decoded.count == BRIDGE_RESOURCE_PAGE_SIZE;
    }
    for (uint8_t i = 0; valid && i < decoded.count; ++i) {
        valid = list_entry(kind, cJSON_GetArrayItem(items, i), &arena, &decoded.entries[i]);
        for (uint8_t other = 0; valid && other < i; ++other) valid = decoded.entries[other].id != decoded.entries[i].id;
    }
    const bool too_large = !valid && arena.exhausted;
    cJSON_Delete(root);
    if (!valid) return too_large ? BRIDGE_RESOURCE_TOO_LARGE : BRIDGE_RESOURCE_INVALID;
    *result = decoded;
    return BRIDGE_RESOURCE_OK;
}

static bool task_status(const cJSON *item, bridge_task_status_t *status)
{
    static const char *const names[] = {"pending", "in_progress", "completed", "canceled"};
    if (!cJSON_IsString(item) || item->valuestring == NULL) return false;
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        if (strcmp(item->valuestring, names[i]) == 0) {
            *status = (bridge_task_status_t)i;
            return true;
        }
    }
    return false;
}

bridge_resource_result_t bridge_resource_decode_detail(bridge_resource_kind_t kind, uint64_t id, int status,
                                                       const char *body, size_t length, char *arena_memory,
                                                       size_t arena_capacity, bridge_resource_detail_t *result)
{
    if (kind >= BRIDGE_RESOURCE_KIND_COUNT || id == 0U || id > BRIDGE_RESOURCE_ID_MAX || body == NULL ||
        arena_memory == NULL || result == NULL) return BRIDGE_RESOURCE_INVALID;
    if (length > BRIDGE_RESOURCE_BODY_MAX_BYTES) return BRIDGE_RESOURCE_TOO_LARGE;
    if (status != 200) return error_result(status, body, length);
    cJSON *root = parse(body, length);
    if (root == NULL) return BRIDGE_RESOURCE_INVALID;

    static const char *const wrapper[] = {"item"};
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, "item");
    arena_t arena = {.next = arena_memory, .remaining = arena_capacity};
    bridge_resource_detail_t decoded = {.kind = kind};
    uint64_t echoed = 0U;
    bool valid = bridge_json_fields_exact(root, wrapper, 1U) &&
                 bridge_json_fields_exact(item, DETAIL_FIELDS[kind], DETAIL_FIELD_COUNT[kind]) &&
                 safe_integer(cJSON_GetObjectItemCaseSensitive(item, "id"), &echoed) && echoed == id &&
                 optional_text(cJSON_GetObjectItemCaseSensitive(item, "channel_id")) &&
                 wire_time(cJSON_GetObjectItemCaseSensitive(item, "created_at"), decoded.created_at, false);
    decoded.id = id;
    if (valid) {
        decoded.text = arena_copy(&arena, cJSON_GetObjectItemCaseSensitive(item, kind == BRIDGE_RESOURCE_ALERTS
                                                                                ? "message" : "title"));
        valid = decoded.text != NULL;
    }
    if (valid && kind != BRIDGE_RESOURCE_ALERTS) {
        const cJSON *tag = cJSON_GetObjectItemCaseSensitive(item, "tag");
        decoded.description = arena_copy(&arena, cJSON_GetObjectItemCaseSensitive(item, "description"));
        valid = decoded.description != NULL && optional_text(tag);
        if (valid && cJSON_IsString(tag)) {
            decoded.tag = arena_copy(&arena, tag);
            valid = decoded.tag != NULL;
        }
    }
    if (valid && kind == BRIDGE_RESOURCE_TASKS)
        valid = task_status(cJSON_GetObjectItemCaseSensitive(item, "status"), &decoded.task_status) &&
                wire_time(cJSON_GetObjectItemCaseSensitive(item, "alert_at"), decoded.when_at, true);
    if (valid && kind == BRIDGE_RESOURCE_NOTES)
        valid = wire_time(cJSON_GetObjectItemCaseSensitive(item, "updated_at"), decoded.updated_at, false);
    if (valid && kind == BRIDGE_RESOURCE_ALERTS) {
        const cJSON *sent = cJSON_GetObjectItemCaseSensitive(item, "sent");
        valid = cJSON_IsFalse(sent) &&
                wire_time(cJSON_GetObjectItemCaseSensitive(item, "scheduled_at"), decoded.when_at, false);
    }
    const bool too_large = !valid && arena.exhausted;
    cJSON_Delete(root);
    if (!valid) return too_large ? BRIDGE_RESOURCE_TOO_LARGE : BRIDGE_RESOURCE_INVALID;
    *result = decoded;
    return BRIDGE_RESOURCE_OK;
}

bool bridge_resource_complete_path(uint64_t id, char *path, size_t capacity)
{
    if (path == NULL || capacity == 0U || id == 0U || id > BRIDGE_RESOURCE_ID_MAX) return false;
    const int length = snprintf(path, capacity, "/v1/device/tasks/%llu/complete", (unsigned long long)id);
    return length > 0 && (size_t)length < capacity;
}

/* An action error decided by the HTTP/code pair. Anything unknown after dispatch may have been applied. */
static bridge_action_result_t action_error(int status, const cJSON *root, bool complete)
{
    static const char *const wrapper[] = {"error"};
    static const char *const fields[] = {"code", "message"};
    const cJSON *error = cJSON_GetObjectItemCaseSensitive(root, "error");
    const cJSON *code = cJSON_GetObjectItemCaseSensitive(error, "code");
    const cJSON *message = cJSON_GetObjectItemCaseSensitive(error, "message");
    if (!bridge_json_fields_exact(root, wrapper, 1U) || !bridge_json_fields_exact(error, fields, 2U) ||
        !cJSON_IsString(code) || code->valuestring == NULL || !cJSON_IsString(message))
        return BRIDGE_ACTION_UNCONFIRMED;
    const char *name = code->valuestring;
    const bool bot_rejection = strcmp(name, "bot_auth_rejected") == 0 || strcmp(name, "bot_access_denied") == 0;
    /* Complete reads the task before updating it: a failed read means no update was sent. */
    const bool read_failed = strcmp(name, "upstream_unavailable") == 0 || strcmp(name, "invalid_upstream_response") == 0;
    if (status == 503 && strcmp(name, "temporarily_unavailable") == 0) return BRIDGE_ACTION_NOT_SENT;
    if (complete && status == 502 && read_failed) return BRIDGE_ACTION_NOT_SENT;
    if (status == 404 && strcmp(name, "resource_not_found") == 0) return BRIDGE_ACTION_NOT_FOUND;
    if (complete && status == 409 && strcmp(name, "task_state_changed") == 0) return BRIDGE_ACTION_CHANGED;
    if ((status == 400 && strcmp(name, "invalid_request") == 0) || (status == 401 && strcmp(name, "unauthorized") == 0) ||
        (status == 404 && strcmp(name, "not_found") == 0) || (status == 502 && bot_rejection))
        return BRIDGE_ACTION_REJECTED;
    return BRIDGE_ACTION_UNCONFIRMED;
}

/* Success is exactly {"id": id, <field>: expected}; expected is a string, or NULL for boolean true. */
static bridge_action_result_t decode_action(uint64_t id, int status, const char *body, size_t length,
                                            const char *field, const char *expected, bool complete)
{
    if (id == 0U || id > BRIDGE_RESOURCE_ID_MAX) return BRIDGE_ACTION_UNCONFIRMED;
    cJSON *root = parse(body, length);
    if (root == NULL) return BRIDGE_ACTION_UNCONFIRMED;
    bridge_action_result_t result = BRIDGE_ACTION_UNCONFIRMED;
    if (status == 200) {
        const char *const names[] = {"id", field};
        const cJSON *value = cJSON_GetObjectItemCaseSensitive(root, field);
        uint64_t answered = 0U;
        const bool matches = expected == NULL ? cJSON_IsTrue(value)
                                              : cJSON_IsString(value) && value->valuestring != NULL &&
                                                    strcmp(value->valuestring, expected) == 0;
        if (bridge_json_fields_exact(root, names, 2U) &&
            safe_integer(cJSON_GetObjectItemCaseSensitive(root, "id"), &answered) && answered == id && matches)
            result = BRIDGE_ACTION_SUCCESS;
    } else {
        result = action_error(status, root, complete);
    }
    cJSON_Delete(root);
    return result;
}

bridge_action_result_t bridge_resource_decode_delete(uint64_t id, int status, const char *body, size_t length)
{
    return decode_action(id, status, body, length, "deleted", NULL, false);
}

bridge_action_result_t bridge_resource_decode_complete(uint64_t id, int status, const char *body, size_t length)
{
    return decode_action(id, status, body, length, "status", "completed", true);
}
