#pragma once

#include <stdbool.h>

#include "esp_err.h"
#include "notification_state.h"

/* Missing key is an empty FIFO; malformed or unreadable data is an error and is never erased. */
esp_err_t notification_journal_load(notification_state_t *state);
/*
 * One NVS commit stores queue order and the pending ACK together. A failure may have committed, so the caller blocks,
 * unless notification_journal_unwritten() says the previous record is intact.
 */
esp_err_t notification_journal_save(const notification_state_t *state);
/*
 * True for save errors treated as not written: allocation, validation or unavailable storage. If NVS ran out of memory
 * after writing, the caller still holds the last saved state and its next save replaces the record.
 */
bool notification_journal_unwritten(esp_err_t error);
