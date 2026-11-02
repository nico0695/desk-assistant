#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "notification_view.h"

typedef void (*notification_view_fn)(const notification_view_t *view);

#define NOTIFICATIONS_LOCAL_UTC_OFFSET_HOURS 3U

/* Restore durable work before opening the bridge stream. Publishes a copied view from the owner task. */
esp_err_t notifications_start(notification_view_fn on_view);
/* SSE and gate callbacks: coalesced, nonblocking invitations. */
void notifications_pending_events(void);
void notifications_bridge_online(bool online);
void notifications_gate_released(void);
/* UI command, queued without storage or network I/O. */
void notifications_close(uint64_t generation, const char *event_id);
/* Returns immediately: true only when one frozen Snooze command was accepted. */
bool notifications_snooze(uint64_t generation, const char *event_id, unsigned preset);
