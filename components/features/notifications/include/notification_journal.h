#pragma once

#include "esp_err.h"
#include "notification_state.h"

/* Missing key is an empty FIFO; malformed or unreadable data is an error and is never erased. */
esp_err_t notification_journal_load(notification_state_t *state);
/* One NVS commit stores queue order and the pending ACK together. Failure may have committed: caller must block. */
esp_err_t notification_journal_save(const notification_state_t *state);
