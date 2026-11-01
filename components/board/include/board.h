#pragma once

#include "esp_err.h"

/* ISR-context notification; the handler only signals its owner task and yields if needed. */
typedef void (*board_isr_cb_t)(void *context);

/*
 * Creates the shared I2C bus, checks the PMIC identity and resets the panel through it.
 * Call once at boot, before any other board function. No PMIC register is written when the identity is wrong.
 */
esp_err_t board_init(void);
