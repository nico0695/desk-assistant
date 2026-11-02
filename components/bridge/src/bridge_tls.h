#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "bridge_origin_store.h"
#include "esp_http_client.h"
#include "esp_transport.h"

/*
 * One HTTPS exchange with the bridge: a TLS transport bounded by an absolute deadline, an HTTP client that never
 * redirects or retries authorization, and the Bearer header. Owned by the task that runs the exchange.
 */
typedef struct {
    esp_transport_handle_t tls;
    esp_transport_handle_t wrapper;
    esp_http_client_handle_t client;
    int64_t deadline_us;
    /* Read budget before streaming; reads past it fail and set wire_limited. */
    size_t wire_bytes;
    size_t wire_limit;
    /* A streaming read reports an expired deadline as a transport timeout instead of a failure. */
    bool streaming;
    bool wire_limited;
    bool timed_out;
    bool encoding_invalid;
    unsigned content_type_count;
    bool content_type_valid;
    /* Optional; when set, every Content-Type header is checked with it. */
    bool (*content_type_accepted)(const char *value);
} bridge_tls_t;

typedef enum {
    BRIDGE_TLS_OPENED,
    /* The shared handshake gate was not acquired within the deadline; nothing was sent. */
    BRIDGE_TLS_GATE_TIMEOUT,
    BRIDGE_TLS_OPEN_FAILED,
} bridge_tls_open_t;

/*
 * Creates the transport and client for url with the origin's Bearer token. The caller sets deadline_us, wire_limit
 * and content_type_accepted first. false on allocation or header failure; call bridge_tls_destroy in every case.
 */
bool bridge_tls_create(bridge_tls_t *session, const bridge_origin_t *origin, const char *url,
                       esp_http_client_method_t method, int timeout_ms);
/* Connects and sends the request headers under the shared handshake gate, released before returning. */
bridge_tls_open_t bridge_tls_open(bridge_tls_t *session, int write_length);
/* Milliseconds left before the deadline, capped at requested_ms when it is not negative. */
int bridge_tls_budget(const bridge_tls_t *session, int requested_ms);
void bridge_tls_destroy(bridge_tls_t *session);
