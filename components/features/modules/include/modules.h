#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "bridge_resources.h"
#include "bridge_snooze.h"
#include "esp_err.h"
#include "modules_view.h"

/* Receives a copied view from any task; it must not block or call back into modules. */
typedef void (*modules_view_fn)(const modules_view_t *view);

/* Creates the state owner. Starts no task: a worker lives only while a read is pending. */
esp_err_t modules_start(modules_view_fn on_view);

/* UI commands: nonblocking, no network or storage on the caller. Stale tickets are ignored. */
void modules_enter(bridge_resource_kind_t kind);
void modules_leave(void);
void modules_previous(uint64_t ticket);
void modules_next(uint64_t ticket);
/* Rereads the shown page or detail. */
void modules_refresh(uint64_t ticket);
/* id must belong to the shown page. */
void modules_open_detail(uint64_t ticket, uint64_t id);
void modules_back(uint64_t ticket);

/*
 * Actions on the detail shown with this ticket, which freezes the target. Each dispatches at most one request and
 * is ignored when the ticket is stale, an action is in flight or the detail must be read again.
 */
void modules_delete(uint64_t ticket);
/* Only for a pending task. */
void modules_complete(uint64_t ticket);
/* Only for an alert; sends the alert time shown with the ticket. */
void modules_snooze(uint64_t ticket, bridge_snooze_preset_t preset);

/*
 * Pass the data shown by the view with this ticket to reader, which must copy what it needs before returning and
 * must not call back into modules. False when the ticket is stale or the view has no such data.
 */
bool modules_read_page(uint64_t ticket, void (*reader)(const bridge_resource_page_t *page, void *context),
                       void *context);
bool modules_read_detail(uint64_t ticket, void (*reader)(const bridge_resource_detail_t *detail, void *context),
                         void *context);
