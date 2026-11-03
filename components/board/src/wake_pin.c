#include "wake_pin.h"

#include "esp_check.h"

static const char *TAG = "wake_pin";

void wake_pin_init(wake_pin_t *wake, gpio_num_t pin, gpio_int_type_t edge)
{
    *wake = (wake_pin_t){.pin = pin, .edge = edge};
}

void wake_pin_on_interrupt(wake_pin_t *wake)
{
    if (wake->armed) {
        gpio_intr_disable(wake->pin);
        wake->fired = true;
    }
}

static esp_err_t arm(wake_pin_t *wake)
{
    ESP_RETURN_ON_ERROR(gpio_intr_disable(wake->pin), TAG, "disable");
    wake->armed = true;
    /* Stays set until the interrupt is live, so a failed arm is retried by the next call. */
    wake->fired = true;
    /* A change between this read and enabling fires at once, so no transition is lost. */
    const gpio_int_type_t level = gpio_get_level(wake->pin) ? GPIO_INTR_LOW_LEVEL : GPIO_INTR_HIGH_LEVEL;
    ESP_RETURN_ON_ERROR(gpio_wakeup_enable(wake->pin, level), TAG, "wakeup");
    wake->fired = false;
    const esp_err_t error = gpio_intr_enable(wake->pin);
    if (error != ESP_OK) {
        wake->fired = true;
    }
    return error;
}

static esp_err_t disarm(wake_pin_t *wake)
{
    ESP_RETURN_ON_ERROR(gpio_intr_disable(wake->pin), TAG, "disable");
    ESP_RETURN_ON_ERROR(gpio_wakeup_disable(wake->pin), TAG, "wakeup");
    ESP_RETURN_ON_ERROR(gpio_set_intr_type(wake->pin, wake->edge), TAG, "edge");
    wake->armed = false;
    wake->fired = false;
    return gpio_intr_enable(wake->pin);
}

esp_err_t wake_pin_set_armed(wake_pin_t *wake, bool armed)
{
    if (armed) {
        return wake->armed && !wake->fired ? ESP_OK : arm(wake);
    }
    return wake->armed ? disarm(wake) : ESP_OK;
}
