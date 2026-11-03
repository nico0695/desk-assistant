#include "board_key.h"

#include "driver/gpio.h"
#include "esp_check.h"
#include "wake_pin.h"

#define KEY_GPIO GPIO_NUM_10
#define KEY_PRESSED_LEVEL 0

static const char *TAG = "board_key";
static bool key_ready;
static wake_pin_t key_wake;
static board_isr_cb_t key_listener;
static void *key_listener_context;

static void on_key_interrupt(void *unused)
{
    (void)unused;
    wake_pin_on_interrupt(&key_wake);
    key_listener(key_listener_context);
}

esp_err_t board_key_init(board_isr_cb_t on_edge, void *context)
{
    ESP_RETURN_ON_FALSE(on_edge != NULL, ESP_ERR_INVALID_ARG, TAG, "callback");
    ESP_RETURN_ON_FALSE(!key_ready, ESP_ERR_INVALID_STATE, TAG, "already initialized");
    /* The board has an external pull-up on KEY. */
    const gpio_config_t config = {
        .pin_bit_mask = 1ULL << KEY_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE,
    };
    key_listener = on_edge;
    key_listener_context = context;
    wake_pin_init(&key_wake, KEY_GPIO, config.intr_type);
    ESP_RETURN_ON_ERROR(gpio_config(&config), TAG, "GPIO");
    ESP_RETURN_ON_ERROR(gpio_isr_handler_add(KEY_GPIO, on_key_interrupt, NULL), TAG, "interrupt");
    key_ready = true;
    return ESP_OK;
}

bool board_key_pressed(void)
{
    return key_ready && gpio_get_level(KEY_GPIO) == KEY_PRESSED_LEVEL;
}

esp_err_t board_key_set_wake_armed(bool armed)
{
    ESP_RETURN_ON_FALSE(key_ready, ESP_ERR_INVALID_STATE, TAG, "not initialized");
    return wake_pin_set_armed(&key_wake, armed);
}
