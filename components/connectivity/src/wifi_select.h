#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "wifi_list.h"

/*
 * Station selection policy. Times are monotonic milliseconds; indexes refer to the saved list. After a list change
 * the caller resets the state, or reports the new index of the connected network with wifi_select_connected.
 */

#define WIFI_SELECT_UNSEEN INT16_MIN
#define WIFI_SELECT_NO_WAKE UINT64_MAX

typedef enum {
    WIFI_SELECT_WAIT,
    WIFI_SELECT_SCAN,
    WIFI_SELECT_CONNECT,
} wifi_select_action_t;

typedef struct {
    uint8_t order[WIFI_LIST_CAPACITY];
    uint8_t count;
    uint8_t next;
    int8_t target;
    bool attempting;
    bool lost;
    bool scan_pending;
    bool full_round;
    bool preferred;
    bool paused;
    uint64_t attempt_end_ms;
    uint64_t lost_end_ms;
    uint64_t retry_ms;
    uint64_t scan_ms;
} wifi_select_t;

/* Starts over with a scan now: boot or list change without a link. Keeps the setup pause. */
void wifi_select_reset(wifi_select_t *state, uint64_t now_ms);

/* Tries one network first, even during setup (a network just saved); its failure leads to a scan. */
void wifi_select_prefer(wifi_select_t *state, uint8_t index, uint64_t now_ms);

/*
 * Setup open: no scans or rounds, so the phone keeps the access point; an attempt in progress, a preferred network
 * and the recovery of a lost link still run. Setup closed: a pending scan runs at once.
 */
void wifi_select_set_paused(wifi_select_t *state, bool paused, uint64_t now_ms);

/* Strongest RSSI per saved network; call once per scanned record. best starts at WIFI_SELECT_UNSEEN. */
void wifi_select_note(const wifi_list_t *list, const char *ssid, int rssi, int16_t best[WIFI_LIST_CAPACITY]);

/* Seen networks strongest first (ties in saved order), then unseen ones in saved order. A failed scan passes all
 * unseen. */
void wifi_select_round(wifi_select_t *state, uint8_t saved, const int16_t best[WIFI_LIST_CAPACITY]);

/* A link with an IP on the network at index. A stable link is never changed for a better signal. */
void wifi_select_connected(wifi_select_t *state, uint8_t index);

/* A lost link retries the same network for a while; a failure during an attempt moves on at once. */
void wifi_select_disconnected(wifi_select_t *state, bool was_connected, uint64_t now_ms);

/* One decision. CONNECT sets *index and starts an attempt; SCAN must be answered with wifi_select_round. */
wifi_select_action_t wifi_select_step(wifi_select_t *state, uint64_t now_ms, uint8_t *index);

/* Earliest time wifi_select_step can decide something new without an event, or WIFI_SELECT_NO_WAKE. */
uint64_t wifi_select_wake_ms(const wifi_select_t *state, uint64_t now_ms);
