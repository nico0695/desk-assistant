#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "connectivity_contract.h"

/*
 * Assembles one USB line in a caller-owned buffer of PROVISION_LINE_MAX_BYTES + 1. CR is ignored; LF ends the
 * line; a line over PROVISION_LINE_MAX_BYTES or with a NUL byte is rejected whole; a partial line idle for
 * PROVISION_LINE_IDLE_MS is dropped. The buffer holds secrets: every reset zeroes it.
 */
typedef struct {
    char *buffer;
    size_t length;
    bool rejected;
    uint64_t last_byte_ms;
} provision_line_t;

typedef enum {
    PROVISION_LINE_PENDING,
    PROVISION_LINE_READY,
    PROVISION_LINE_REJECTED,
} provision_line_status_t;

void provision_line_init(provision_line_t *line, char *buffer);

/* READY leaves a NUL-terminated line in the buffer until provision_line_clear. REJECTED has already cleared it. */
provision_line_status_t provision_line_feed(provision_line_t *line, char byte, uint64_t now_ms);

/* Drops a partial line idle for PROVISION_LINE_IDLE_MS; call when a read times out. */
void provision_line_expire(provision_line_t *line, uint64_t now_ms);

void provision_line_clear(provision_line_t *line);

typedef enum {
    PROVISION_OP_CONFIGURE_BRIDGE,
    PROVISION_OP_CONFIGURE,
} provision_op_t;

/* Strings point into the line buffer and live until provision_line_clear. Values are not validated here. */
typedef struct {
    provision_op_t op;
    const char *wifi_ssid;
    const char *wifi_password;
    const char *bridge_url;
    const char *device_token;
    uint32_t screen_timeout_s;
} provision_request_t;

/*
 * Parses one flat JSON object in place, decoding strings inside the buffer. Requires version 1 and exactly the
 * fields of the operation: configure_bridge {v, op, bridge_url, device_token}; configure adds wifi_ssid,
 * wifi_password and screen_timeout_s. Strings may not contain NUL.
 */
bool provision_request_parse(char *line, provision_request_t *request);
