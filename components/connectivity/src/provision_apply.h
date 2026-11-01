#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "provision_line.h"

/* Durable effects of one provisioning request, each owned by its component; true when stored. */
typedef struct {
    bool (*timeout_valid)(uint32_t seconds);
    bool (*save_origin)(const char *url, const char *token);
    bool (*store_timeout)(uint8_t seconds);
    bool (*save_network)(const char *ssid, const char *password);
} provision_actions_t;

/*
 * Validates every value before writing anything, then writes the origin, the screen timeout and the network, in
 * that order, so the connection only starts when the rest is stored. Stops at the first failed write; what was
 * already written stays. True only when everything was stored.
 */
bool provision_apply(const provision_request_t *request, const provision_actions_t *actions);
