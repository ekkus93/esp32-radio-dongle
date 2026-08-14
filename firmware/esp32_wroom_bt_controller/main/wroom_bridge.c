#include "wroom_bridge.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "driver/uart.h"
#include "esp_bt.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "radio_h4.h"
#include "radio_link_config.h"

#define WROOM_HCI_UART UART_NUM_2
#define WROOM_HCI_TX_GPIO 17
#define WROOM_HCI_RX_GPIO 16
#define WROOM_HCI_RTS_GPIO 26
#define WROOM_HCI_CTS_GPIO 25

#define WROOM_UART_RX_BUFFER_SIZE 4096
#define WROOM_UART_TX_BUFFER_SIZE 4096
#define WROOM_UART_EVENT_QUEUE_SIZE 20
#define WROOM_UART_RX_FLOW_THRESHOLD 96
#define WROOM_UART_READ_CHUNK 256
#define WROOM_QUEUE_CAPACITY RADIO_H4_QUEUE_CAPACITY
#define WROOM_VHCI_READY_TIMEOUT_MS 1000
#define WROOM_RECOVERY_DELAY_MS 100
#define WROOM_DIAGNOSTIC_PERIOD_MS 5000

static const char *TAG = "wroom_bridge";

typedef enum {
    WROOM_STATE_RESET = 0,
    WROOM_STATE_INITIALIZING,
    WROOM_STATE_TRANSPORT_READY,
    WROOM_STATE_OPERATIONAL,
    WROOM_STATE_RECOVERING,
} wroom_state_t;

typedef struct {
    uint32_t host_to_controller_packets;
    uint32_t controller_to_host_packets;
    uint32_t malformed_host_packets;
    uint32_t malformed_controller_packets;
    uint32_t host_to_controller_queue_full;
    uint32_t controller_to_host_queue_full;
    uint32_t uart_errors;
    uint32_t recovery_requests;
    UBaseType_t host_to_controller_high_water;
    UBaseType_t controller_to_host_high_water;
} wroom_diagnostics_t;

static volatile wroom_state_t s_state = WROOM_STATE_RESET;
static wroom_diagnostics_t s_diag;
static QueueHandle_t s_host_to_controller_queue;
static QueueHandle_t s_controller_to_host_queue;
static QueueHandle_t s_uart_event_queue;
static TaskHandle_t s_controller_tx_task;
static TaskHandle_t s_recovery_task;
static radio_h4_parser_t s_uart_rx_parser;
static radio_h4_packet_t s_uart_rx_packet;
static radio_h4_packet_t s_vhci_rx_packet;

static const char *state_name(wroom_state_t state)
{
    switch (state) {
    case WROOM_STATE_RESET:
        return "RESET";
    case WROOM_STATE_INITIALIZING:
        return "INITIALIZING";
    case WROOM_STATE_TRANSPORT_READY:
        return "TRANSPORT_READY";
    case WROOM_STATE_OPERATIONAL:
        return "OPERATIONAL";
    case WROOM_STATE_RECOVERING:
        return "RECOVERING";
    default:
        return "UNKNOWN";
    }
}

static void set_state(wroom_state_t state)
{
    const wroom_state_t previous = s_state;
    s_state = state;
    ESP_LOGI(TAG, "state %s -> %s", state_name(previous), state_name(state));
}

static void request_recovery(const char *reason)
{
    if (s_state != WROOM_STATE_RECOVERING) {
        s_diag.recovery_requests++;
        ESP_LOGE(TAG, "fatal bridge condition: %s", reason);
        set_state(WROOM_STATE_RECOVERING);
    }
    if (s_recovery_task != NULL) {
        xTaskNotifyGive(s_recovery_task);
    }
}

static void update_high_water(QueueHandle_t queue, UBaseType_t *high_water)
{
    const UBaseType_t depth = uxQueueMessagesWaiting(queue);
    if (depth > *high_water) {
        *high_water = depth;
    }
}

static esp_err_t init_nvs(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG, "erase NVS");
        err = nvs_flash_init();
    }
    return err;
}

static esp_err_t init_uart(void)
{
    const uart_config_t config = {
        .baud_rate = RADIO_HCI_UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_CTS_RTS,
        .rx_flow_ctrl_thresh = WROOM_UART_RX_FLOW_THRESHOLD,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_RETURN_ON_ERROR(uart_driver_install(WROOM_HCI_UART, WROOM_UART_RX_BUFFER_SIZE,
                                            WROOM_UART_TX_BUFFER_SIZE,
                                            WROOM_UART_EVENT_QUEUE_SIZE, &s_uart_event_queue, 0),
                        TAG, "install HCI UART driver");
    ESP_RETURN_ON_ERROR(uart_param_config(WROOM_HCI_UART, &config), TAG,
                        "configure HCI UART");
    ESP_RETURN_ON_ERROR(uart_set_pin(WROOM_HCI_UART, WROOM_HCI_TX_GPIO, WROOM_HCI_RX_GPIO,
                                     WROOM_HCI_RTS_GPIO, WROOM_HCI_CTS_GPIO),
                        TAG, "route HCI UART pins");
    ESP_RETURN_ON_ERROR(uart_flush_input(WROOM_HCI_UART), TAG, "flush HCI UART input");

    ESP_LOGI(TAG, "UART2 H4: baud=%d TX=%d RX=%d RTS=%d CTS=%d", RADIO_HCI_UART_BAUD,
             WROOM_HCI_TX_GPIO, WROOM_HCI_RX_GPIO, WROOM_HCI_RTS_GPIO,
             WROOM_HCI_CTS_GPIO);
    return ESP_OK;
}

static esp_err_t init_controller(void)
{
    esp_bt_controller_config_t config = BT_CONTROLLER_INIT_CONFIG_DEFAULT();

    ESP_RETURN_ON_ERROR(esp_bt_controller_init(&config), TAG, "initialize BT controller");
    ESP_RETURN_ON_ERROR(esp_bt_controller_enable(ESP_BT_MODE_BTDM), TAG,
                        "enable dual-mode BT controller");
    ESP_RETURN_ON_ERROR(esp_bredr_sco_datapath_set(ESP_SCO_DATA_PATH_HCI), TAG,
                        "select HCI SCO data path");

    if (esp_bt_controller_get_status() != ESP_BT_CONTROLLER_STATUS_ENABLED) {
        ESP_LOGE(TAG, "BT controller did not reach ENABLED state");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Bluetooth controller enabled in BR/EDR + BLE mode");
    return ESP_OK;
}

static BaseType_t enqueue_host_to_controller(const radio_h4_packet_t *packet)
{
    if (xQueueSend(s_host_to_controller_queue, packet, 0) != pdTRUE) {
        s_diag.host_to_controller_queue_full++;
        request_recovery("host-to-controller queue exhausted");
        return pdFALSE;
    }
    s_diag.host_to_controller_packets++;
    update_high_water(s_host_to_controller_queue, &s_diag.host_to_controller_high_water);
    return pdTRUE;
}

static BaseType_t enqueue_controller_to_host(const radio_h4_packet_t *packet)
{
    if (xQueueSend(s_controller_to_host_queue, packet, 0) != pdTRUE) {
        s_diag.controller_to_host_queue_full++;
        request_recovery("controller-to-host queue exhausted");
        return pdFALSE;
    }
    s_diag.controller_to_host_packets++;
    update_high_water(s_controller_to_host_queue, &s_diag.controller_to_host_high_water);
    return pdTRUE;
}

static void vhci_notify_send_available(void)
{
    if (s_controller_tx_task != NULL) {
        xTaskNotifyGive(s_controller_tx_task);
    }
}

static int vhci_notify_host_recv(uint8_t *data, uint16_t len)
{
    const radio_h4_result_t validation = radio_h4_validate_complete(data, len);
    if (validation != RADIO_H4_OK) {
        s_diag.malformed_controller_packets++;
        request_recovery("malformed H4 packet from Bluetooth controller");
        return ESP_FAIL;
    }

    memcpy(s_vhci_rx_packet.bytes, data, len);
    s_vhci_rx_packet.len = len;
    return enqueue_controller_to_host(&s_vhci_rx_packet) == pdTRUE ? ESP_OK : ESP_FAIL;
}

static const esp_vhci_host_callback_t s_vhci_callbacks = {
    .notify_host_send_available = vhci_notify_send_available,
    .notify_host_recv = vhci_notify_host_recv,
};

static void uart_rx_task(void *arg)
{
    (void)arg;
    uint8_t input[WROOM_UART_READ_CHUNK];
    radio_h4_parser_init(&s_uart_rx_parser);

    for (;;) {
        const int received = uart_read_bytes(WROOM_HCI_UART, input, sizeof(input),
                                             pdMS_TO_TICKS(20));
        if (received < 0) {
            s_diag.uart_errors++;
            request_recovery("UART read failed");
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        if (received == 0) {
            continue;
        }

        size_t offset = 0u;
        while (offset < (size_t)received && s_state != WROOM_STATE_RECOVERING) {
            size_t consumed = 0u;
            const radio_h4_result_t result =
                radio_h4_parser_feed(&s_uart_rx_parser, input + offset,
                                     (size_t)received - offset, &consumed, &s_uart_rx_packet);
            offset += consumed;

            if (result == RADIO_H4_PACKET_READY) {
                if (enqueue_host_to_controller(&s_uart_rx_packet) != pdTRUE) {
                    break;
                }
                continue;
            }
            if (result == RADIO_H4_OK) {
                break;
            }

            s_diag.malformed_host_packets++;
            request_recovery("malformed H4 stream from ESP32-S3");
            break;
        }
    }
}

static void controller_tx_task(void *arg)
{
    (void)arg;
    static radio_h4_packet_t packet;

    for (;;) {
        if (xQueueReceive(s_host_to_controller_queue, &packet, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        while (!esp_vhci_host_check_send_available()) {
            if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(WROOM_VHCI_READY_TIMEOUT_MS)) == 0u) {
                request_recovery("Bluetooth controller HCI transmit stalled");
                break;
            }
        }
        if (s_state == WROOM_STATE_RECOVERING) {
            continue;
        }

        esp_vhci_host_send_packet(packet.bytes, (uint16_t)packet.len);
    }
}

static void uart_tx_task(void *arg)
{
    (void)arg;
    static radio_h4_packet_t packet;

    for (;;) {
        if (xQueueReceive(s_controller_to_host_queue, &packet, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        const int written = uart_write_bytes(WROOM_HCI_UART, packet.bytes, packet.len);
        if (written != (int)packet.len) {
            s_diag.uart_errors++;
            request_recovery("UART write failed or was partial");
        }
    }
}

static void uart_event_task(void *arg)
{
    (void)arg;
    uart_event_t event;

    for (;;) {
        if (xQueueReceive(s_uart_event_queue, &event, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        switch (event.type) {
        case UART_FIFO_OVF:
        case UART_BUFFER_FULL:
        case UART_BREAK:
        case UART_PARITY_ERR:
        case UART_FRAME_ERR:
            s_diag.uart_errors++;
            request_recovery("UART transport error event");
            break;
        default:
            break;
        }
    }
}

static void diagnostic_task(void *arg)
{
    (void)arg;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(WROOM_DIAGNOSTIC_PERIOD_MS));
        ESP_LOGI(TAG,
                 "diag state=%s h2c=%" PRIu32 " c2h=%" PRIu32
                 " malformed=%" PRIu32 "/%" PRIu32 " qfull=%" PRIu32 "/%" PRIu32
                 " qhigh=%u/%u uart_err=%" PRIu32 " recoveries=%" PRIu32,
                 state_name(s_state), s_diag.host_to_controller_packets,
                 s_diag.controller_to_host_packets, s_diag.malformed_host_packets,
                 s_diag.malformed_controller_packets, s_diag.host_to_controller_queue_full,
                 s_diag.controller_to_host_queue_full,
                 (unsigned)s_diag.host_to_controller_high_water,
                 (unsigned)s_diag.controller_to_host_high_water, s_diag.uart_errors,
                 s_diag.recovery_requests);
    }
}

static void recovery_task(void *arg)
{
    (void)arg;
    for (;;) {
        (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        ESP_LOGE(TAG, "controlled recovery: restarting WROOM bridge MCU");
        vTaskDelay(pdMS_TO_TICKS(WROOM_RECOVERY_DELAY_MS));
        esp_restart();
    }
}

static esp_err_t create_queues_and_recovery_task(void)
{
    s_host_to_controller_queue = xQueueCreate(WROOM_QUEUE_CAPACITY, sizeof(radio_h4_packet_t));
    s_controller_to_host_queue = xQueueCreate(WROOM_QUEUE_CAPACITY, sizeof(radio_h4_packet_t));
    if (s_host_to_controller_queue == NULL || s_controller_to_host_queue == NULL) {
        return ESP_ERR_NO_MEM;
    }

    if (xTaskCreate(recovery_task, "wroom_recovery", 3072, NULL, 12, &s_recovery_task) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

static esp_err_t create_transport_tasks(void)
{
    if (xTaskCreate(controller_tx_task, "hci_to_ctrl", 3072, NULL, 10,
                    &s_controller_tx_task) != pdPASS ||
        xTaskCreate(uart_rx_task, "hci_uart_rx", 3072, NULL, 10, NULL) != pdPASS ||
        xTaskCreate(uart_tx_task, "hci_uart_tx", 3072, NULL, 10, NULL) != pdPASS ||
        xTaskCreate(uart_event_task, "hci_uart_evt", 3072, NULL, 11, NULL) != pdPASS ||
        xTaskCreate(diagnostic_task, "hci_diag", 3072, NULL, 4, NULL) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t wroom_bridge_start(void)
{
    memset(&s_diag, 0, sizeof(s_diag));
    set_state(WROOM_STATE_INITIALIZING);

    ESP_RETURN_ON_ERROR(create_queues_and_recovery_task(), TAG,
                        "allocate HCI queues/recovery task");
    ESP_RETURN_ON_ERROR(init_nvs(), TAG, "initialize NVS");
    ESP_RETURN_ON_ERROR(init_uart(), TAG, "initialize inter-MCU UART");
    ESP_RETURN_ON_ERROR(init_controller(), TAG, "initialize Bluetooth controller");
    ESP_RETURN_ON_ERROR(create_transport_tasks(), TAG, "create HCI transport tasks");
    ESP_RETURN_ON_ERROR(esp_vhci_host_register_callback(&s_vhci_callbacks), TAG,
                        "register VHCI callbacks");

    set_state(WROOM_STATE_TRANSPORT_READY);
    set_state(WROOM_STATE_OPERATIONAL);
    ESP_LOGI(TAG, "WROOM Bluetooth controller bridge ready");
    return ESP_OK;
}
