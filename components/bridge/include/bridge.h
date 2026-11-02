#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

/* Called from bridge or event tasks; must not wait. Newer generations supersede older ones. */
typedef void (*bridge_online_fn)(bool online, uint64_t generation);

typedef struct {
    bridge_online_fn online_changed;
    /* The event stream signaled a result for the watched assistant attempt. From the stream task; must not wait. */
    void (*assistant_result)(void);
    /* A coalesced invitation to drain; delivered on stream open and pending_events frames. */
    void (*pending_events)(void);
} bridge_hooks_t;

/* Start after storage and before connectivity. Online and assistant hooks are required. */
esp_err_t bridge_start(const bridge_hooks_t *hooks);
/* Nonblocking and safe from the Wi-Fi event loop. */
void bridge_set_ip(bool has_ip);
/* Watches the assistant attempt id on the event stream, replacing any previous one; NULL stops watching. */
void bridge_watch_assistant(const char *id);
