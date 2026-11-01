#pragma once

#include "esp_err.h"
#include "provision_apply.h"

/*
 * Installs the USB Serial/JTAG driver, keeps the console log on it, and starts the task that reads one JSON request
 * per line, applies it and replies. actions must outlive the task.
 */
esp_err_t provision_usb_start(const provision_actions_t *actions);
