#include "bridge_origin_store.h"

#include <string.h>

#include "storage.h"

/* A new namespace marks an incompatible record; the old one is left untouched. */
#define BRIDGE_NAMESPACE "bridge_v1"
#define BRIDGE_ORIGIN_KEY "origin"

static bridge_origin_changed_fn origin_changed;

void bridge_origin_set_changed_callback(bridge_origin_changed_fn callback)
{
    origin_changed = callback;
}

esp_err_t bridge_origin_save(const char *url, const char *token)
{
    if (!bridge_url_valid(url) || !bridge_token_valid(token)) {
        return ESP_ERR_INVALID_ARG;
    }
    bridge_origin_t origin = {0};
    memcpy(origin.url, url, strlen(url));
    memcpy(origin.token, token, strlen(token));
    const esp_err_t error = storage_set_blob(BRIDGE_NAMESPACE, BRIDGE_ORIGIN_KEY, &origin, sizeof(origin));
    if (error == ESP_OK && origin_changed != NULL) {
        origin_changed(&origin);
    }
    memset(&origin, 0, sizeof(origin));
    return error;
}

esp_err_t bridge_origin_load(bridge_origin_t *origin)
{
    esp_err_t error = storage_get_blob(BRIDGE_NAMESPACE, BRIDGE_ORIGIN_KEY, origin, sizeof(*origin));
    if (error == ESP_OK && (!bridge_url_valid(origin->url) || !bridge_token_valid(origin->token))) {
        error = ESP_ERR_INVALID_STATE;
    }
    if (error != ESP_OK) {
        memset(origin, 0, sizeof(*origin));
    }
    return error;
}
