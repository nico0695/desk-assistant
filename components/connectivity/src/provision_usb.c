#include "provision_usb.h"

#include <string.h>

#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "provision_line.h"
#include "sdkconfig.h"

#define USB_TASK_NAME "usb_provision"
#define READ_CHUNK_BYTES 64U
#define REPLY_TIMEOUT_MS 1000U
#define MICROSECONDS_PER_MILLISECOND 1000

static const char *TAG = "usb_provision";
_Static_assert(PROVISION_PROTOCOL_VERSION == 1U, "the replies below carry version 1");
static const char REPLY_OK[] = "{\"v\":1,\"ok\":true}\n";
static const char REPLY_ERROR[] = "{\"v\":1,\"ok\":false,\"error\":\"invalid configuration or storage failure\"}\n";

static const provision_actions_t *provision_actions;
/* Owned by the USB task. Holds secrets; provision_line clears it after every line. */
static char line_buffer[PROVISION_LINE_MAX_BYTES + 1];

static uint64_t uptime_ms(void)
{
    return (uint64_t)(esp_timer_get_time() / MICROSECONDS_PER_MILLISECOND);
}

static void reply(bool ok)
{
    const char *text = ok ? REPLY_OK : REPLY_ERROR;
    usb_serial_jtag_write_bytes(text, strlen(text), pdMS_TO_TICKS(REPLY_TIMEOUT_MS));
}

/* Logs the operation and the outcome only: never a received field. */
static void handle_line(void)
{
    provision_request_t request;
    const bool parsed = provision_request_parse(line_buffer, &request);
    const bool ok = parsed && provision_apply(&request, provision_actions);
    const provision_op_t op = request.op;
    memset(&request, 0, sizeof(request));
    if (!parsed) {
        ESP_LOGW(TAG, "request rejected");
    } else {
        ESP_LOGI(TAG, "%s %s", op == PROVISION_OP_CONFIGURE ? "configure" : "configure_bridge",
                 ok ? "stored" : "failed");
    }
    reply(ok);
}

static void run(void *unused)
{
    (void)unused;
    provision_line_t line;
    provision_line_init(&line, line_buffer);
    char chunk[READ_CHUNK_BYTES];
    for (;;) {
        const int received = usb_serial_jtag_read_bytes(chunk, sizeof(chunk), pdMS_TO_TICKS(PROVISION_LINE_IDLE_MS));
        const uint64_t now_ms = uptime_ms();
        if (received <= 0) {
            provision_line_expire(&line, now_ms);
            continue;
        }
        for (int i = 0; i < received; ++i) {
            switch (provision_line_feed(&line, chunk[i], now_ms)) {
            case PROVISION_LINE_READY:
                handle_line();
                provision_line_clear(&line);
                break;
            case PROVISION_LINE_REJECTED:
                ESP_LOGW(TAG, "line rejected");
                reply(false);
                break;
            case PROVISION_LINE_PENDING:
                break;
            }
        }
        memset(chunk, 0, sizeof(chunk));
    }
}

esp_err_t provision_usb_start(const provision_actions_t *actions)
{
    provision_actions = actions;
    if (!usb_serial_jtag_is_driver_installed()) {
        usb_serial_jtag_driver_config_t config = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
        config.rx_buffer_size = CONFIG_CONNECTIVITY_USB_RX_BUFFER_SIZE;
        ESP_RETURN_ON_ERROR(usb_serial_jtag_driver_install(&config), TAG, "driver install failed");
    }
    usb_serial_jtag_vfs_use_driver();
    ESP_RETURN_ON_FALSE(xTaskCreate(run, USB_TASK_NAME, CONFIG_CONNECTIVITY_USB_TASK_STACK_SIZE, NULL,
                                    CONFIG_CONNECTIVITY_USB_TASK_PRIORITY, NULL) == pdPASS,
                        ESP_ERR_NO_MEM, TAG, "no memory for the USB task");
    return ESP_OK;
}
