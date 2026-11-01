#include "provision_apply.h"

#include "bridge_origin.h"
#include "wifi_list.h"

static bool request_valid(const provision_request_t *request, const provision_actions_t *actions)
{
    if (!bridge_url_valid(request->bridge_url) || !bridge_token_valid(request->device_token)) {
        return false;
    }
    if (request->op == PROVISION_OP_CONFIGURE_BRIDGE) {
        return true;
    }
    return wifi_ssid_valid(request->wifi_ssid) && wifi_password_valid(request->wifi_password) &&
           actions->timeout_valid(request->screen_timeout_s);
}

bool provision_apply(const provision_request_t *request, const provision_actions_t *actions)
{
    if (!request_valid(request, actions) ||
        !actions->save_origin(request->bridge_url, request->device_token)) {
        return false;
    }
    if (request->op == PROVISION_OP_CONFIGURE_BRIDGE) {
        return true;
    }
    return actions->store_timeout((uint8_t)request->screen_timeout_s) &&
           actions->save_network(request->wifi_ssid, request->wifi_password);
}
