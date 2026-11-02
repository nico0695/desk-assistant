#include "bridge_snooze_send.h"

#include <string.h>

bridge_http_mutation_t bridge_snooze_send(const bridge_origin_t *origin, uint64_t alert_id,
                                          bridge_snooze_preset_t preset, const char *scheduled_at, char *body,
                                          size_t capacity)
{
    char path[BRIDGE_SNOOZE_PATH_SIZE], request[BRIDGE_SNOOZE_REQUEST_SIZE];
    if (!bridge_snooze_path(alert_id, path, sizeof(path)) ||
        !bridge_snooze_request(preset, scheduled_at, request, sizeof(request)))
        return (bridge_http_mutation_t){.response.result = BRIDGE_HTTP_INVALID_ARGUMENT, .unsent = true};
    return bridge_http_json(origin, path, BRIDGE_HTTP_POST_JSON, request, strlen(request),
                            BRIDGE_RESOURCE_ACTION_GATE_WAIT_MS, BRIDGE_RESOURCE_IO_MS, body, capacity);
}
