#pragma once

#include <stdbool.h>

#include "driver/gpio.h"
#include "esp_err.h"

/*
 * An input that interrupts on edges while the panel is on and, while armed, on the level opposite to the one it had
 * when armed, which is also a light-sleep wakeup source. An armed interrupt fires once: the ISR disables it so a
 * held level cannot storm, and arming again (from a task) re-reads the level.
 */
typedef struct {
    gpio_num_t pin;
    gpio_int_type_t edge;
    volatile bool armed;
    volatile bool fired;
} wake_pin_t;

void wake_pin_init(wake_pin_t *wake, gpio_num_t pin, gpio_int_type_t edge);

/* Call first in the pin's ISR. */
void wake_pin_on_interrupt(wake_pin_t *wake);

/* Arms, re-arms after a fire, or returns the pin to its edge interrupt. Does nothing when already in that state. */
esp_err_t wake_pin_set_armed(wake_pin_t *wake, bool armed);
