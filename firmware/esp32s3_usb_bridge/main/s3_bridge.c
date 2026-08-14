#include "s3_bridge.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "driver/uart.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "radio_h4.h"
#include "radio_link_config.h"
#include "radio_usb_bth.h"
#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "tusb.h"
#include "usb_descriptors.h"

#define S3_HCI_UART UART_NUM_1
#define S3_HCI_TX_GPIO 4
#define S3_HCI_RX_GPIO 5
#define S3_HCI_RTS_GPIO 6
#define S3_HCI_CTS_GPIO 7

#define S3_UART_RX_BUFFER_SIZE 4096
#define S3_UART_TX_BUFFER_SIZE 4096
#define S3_UART_EVENT_QUEUE_SIZE 20
#define S3_UART_RX_FLOW_THRESHOLD 96
#define S3_UART_READ_CHUNK 256
#define S3_QUEUE_CAPACITY RADIO_H4_QUEUE_CAPACITY

#define S3_CONTROLLER_TX_TIMEOUT_MS 2000
#define S3_CONTROLLER_PROBE_TX_TIMEOUT_MS 5000
#define S3_CONTROLLER_PROBE_RESPONSE_TIMEOUT_MS 2000
#define S3_CONTROLLER_PROBE_READ_SLICE_MS 50
#define S3_RECOVERY_DELAY_MS 100
#define S3_DIAGNOSTIC_PERIOD_MS 5000
#define S3_USB_SEND_RETRY_MS 20

static const char *TAG = "s3_bridge";

typedef enum {
    S3_STATE_RESET = 0,
    S3_STATE_INITIALIZING,
    S3_STATE_WAIT_CONTROLLER,
    S3_STATE_CONTROLLER_READY,
    S3_STATE_USB_ENUMERATING,
    S3_STATE_OPERATIONAL,
    S3_STATE_SUSPENDED,
    S3_STATE_RECOVERING,
} s3_state_t;

typedef struct {
    uint32_t usb_commands;
    uint32_t usb_acl_out;
    uint32_t uart_events;
    uint32_t uart_acl_in;
    uint32_t unsupported_sco;
    uint32_t malformed_packets;
    uint32_t uart_errors;
    uint32_t host_to_controller_queue_full;
    uint32_t event_to_host_queue_full;
    uint32_t acl_to_host_queue_full;
    uint32_t usb_attached;
    uint32_t usb_detached;
    uint32_t usb_suspended;
    uint32_t usb_resumed;
    uint32_t usb_protocol_errors;
    uint32_t recoveries;
    UBaseType_t host_to_controller_high_water;
    UBaseType_t event_to_host_high_water;
    UBaseType_t acl_to_host_high_water;
} s3_diagnostics_t;

static volatile s3_state_t s_state = S3_STATE_RESET;
static s3_diagnostics_t s_diag;
static QueueHandle_t s_host_to_controller_queue;
static QueueHandle_t s_event_to_host_queue;
static QueueHandle_t s_acl_to_host_queue;
static QueueHandle_t s_uart_event_queue;
static TaskHandle_t s_event_usb_task;
static TaskHandle_t s_acl_usb_task;
static TaskHandle_t s_recovery_task;
static radio_h4_parser_t s_uart_parser;
static radio_h4_packet_t s_uart_packet;
static radio_h4_packet_t s_usb_rx_packet;
static bool s_usb_installed;

static const char *state_name(s3_state_t state) {
    switch (state) {
    case S3_STATE_RESET:
        return "RESET";
    case S3_STATE_INITIALIZING:
        return "INITIALIZING";
    case S3_STATE_WAIT_CONTROLLER:
        return "WAIT_CONTROLLER";
    case S3_STATE_CONTROLLER_READY:
        return "CONTROLLER_READY";
    case S3_STATE_USB_ENUMERATING:
        return "USB_ENUMERATING";
    case S3_STATE_OPERATIONAL:
        return "OPERATIONAL";
    case S3_STATE_SUSPENDED:
        return "SUSPENDED";
    case S3_STATE_RECOVERING:
        return "RECOVERING";
    default:
        return "UNKNOWN";
    }
}

static void set_state(s3_state_t state) {
    const s3_state_t previous = s_state;
    s_state = state;
    ESP_LOGI(TAG, "state %s -> %s", state_name(previous), state_name(state));
}

static void request_recovery(const char *reason) {
    if (s_state != S3_STATE_RECOVERING) {
        s_diag.recoveries++;
        ESP_LOGE(TAG, "fatal bridge condition: %s", reason);
        set_state(S3_STATE_RECOVERING);
    }
    if (s_recovery_task != NULL) {
        xTaskNotifyGive(s_recovery_task);
    }
}

static void update_high_water(QueueHandle_t queue, UBaseType_t *high_water) {
    const UBaseType_t depth = uxQueueMessagesWaiting(queue);
    if (depth > *high_water) {
        *high_water = depth;
    }
}

static esp_err_t init_uart(void) {
    const uart_config_t config = {
        .baud_rate = RADIO_HCI_UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_CTS_RTS,
        .rx_flow_ctrl_thresh = S3_UART_RX_FLOW_THRESHOLD,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_RETURN_ON_ERROR(uart_driver_install(S3_HCI_UART, S3_UART_RX_BUFFER_SIZE,
                                            S3_UART_TX_BUFFER_SIZE, S3_UART_EVENT_QUEUE_SIZE,
                                            &s_uart_event_queue, 0),
                        TAG, "install HCI UART driver");
    ESP_RETURN_ON_ERROR(uart_param_config(S3_HCI_UART, &config), TAG, "configure HCI UART");
    ESP_RETURN_ON_ERROR(
        uart_set_pin(S3_HCI_UART, S3_HCI_TX_GPIO, S3_HCI_RX_GPIO, S3_HCI_RTS_GPIO, S3_HCI_CTS_GPIO),
        TAG, "route HCI UART pins");
    ESP_RETURN_ON_ERROR(uart_flush_input(S3_HCI_UART), TAG, "flush HCI UART input");

    ESP_LOGI(TAG, "UART1 H4: baud=%d TX=%d RX=%d RTS=%d CTS=%d", RADIO_HCI_UART_BAUD,
             S3_HCI_TX_GPIO, S3_HCI_RX_GPIO, S3_HCI_RTS_GPIO, S3_HCI_CTS_GPIO);
    return ESP_OK;
}

static bool command_complete_matches(const radio_h4_packet_t *packet, uint16_t opcode) {
    return packet != NULL && packet->len >= 7u && packet->bytes[0] == RADIO_H4_TYPE_EVENT &&
           packet->bytes[1] == 0x0eu && packet->bytes[2] >= 4u &&
           packet->bytes[4] == (uint8_t)(opcode & 0xffu) &&
           packet->bytes[5] == (uint8_t)(opcode >> 8) && packet->bytes[6] == 0x00u;
}

static esp_err_t send_probe_command(const uint8_t *command, size_t command_len, uint16_t opcode,
                                    radio_h4_packet_t *response) {
    radio_h4_parser_t parser;
    uint8_t input[64];
    radio_h4_parser_init(&parser);

    const int written = uart_write_bytes(S3_HCI_UART, command, command_len);
    if (written != (int)command_len) {
        return ESP_FAIL;
    }
    ESP_RETURN_ON_ERROR(
        uart_wait_tx_done(S3_HCI_UART, pdMS_TO_TICKS(S3_CONTROLLER_PROBE_TX_TIMEOUT_MS)), TAG,
        "controller probe UART transmit timeout");

    const unsigned read_iterations =
        S3_CONTROLLER_PROBE_RESPONSE_TIMEOUT_MS / S3_CONTROLLER_PROBE_READ_SLICE_MS;
    for (unsigned iteration = 0u; iteration < read_iterations; ++iteration) {
        const int received = uart_read_bytes(S3_HCI_UART, input, sizeof(input),
                                             pdMS_TO_TICKS(S3_CONTROLLER_PROBE_READ_SLICE_MS));
        if (received < 0) {
            return ESP_FAIL;
        }
        if (received == 0) {
            continue;
        }

        size_t offset = 0u;
        while (offset < (size_t)received) {
            size_t consumed = 0u;
            const radio_h4_result_t result = radio_h4_parser_feed(
                &parser, input + offset, (size_t)received - offset, &consumed, response);
            offset += consumed;
            if (result == RADIO_H4_PACKET_READY) {
                if (command_complete_matches(response, opcode)) {
                    return ESP_OK;
                }
                continue;
            }
            if (result != RADIO_H4_OK) {
                return ESP_ERR_INVALID_RESPONSE;
            }
            break;
        }
    }

    return ESP_ERR_TIMEOUT;
}

static esp_err_t probe_controller(void) {
    static const uint8_t hci_reset[] = {RADIO_H4_TYPE_COMMAND, 0x03, 0x0c, 0x00};
    static const uint8_t read_local_version[] = {RADIO_H4_TYPE_COMMAND, 0x01, 0x10, 0x00};
    radio_h4_packet_t response;

    set_state(S3_STATE_WAIT_CONTROLLER);
    ESP_RETURN_ON_ERROR(uart_flush_input(S3_HCI_UART), TAG, "flush before controller probe");
    ESP_RETURN_ON_ERROR(send_probe_command(hci_reset, sizeof(hci_reset), 0x0c03u, &response), TAG,
                        "HCI Reset probe failed");
    ESP_RETURN_ON_ERROR(
        send_probe_command(read_local_version, sizeof(read_local_version), 0x1001u, &response), TAG,
        "HCI Read Local Version probe failed");

    if (response.len < 15u) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    const uint16_t hci_revision = (uint16_t)response.bytes[8] | ((uint16_t)response.bytes[9] << 8);
    const uint16_t manufacturer =
        (uint16_t)response.bytes[11] | ((uint16_t)response.bytes[12] << 8);
    const uint16_t lmp_subversion =
        (uint16_t)response.bytes[13] | ((uint16_t)response.bytes[14] << 8);
    ESP_LOGI(TAG,
             "WROOM controller probe passed: hci_version=0x%02x hci_revision=0x%04x "
             "lmp_version=0x%02x manufacturer=0x%04x lmp_subversion=0x%04x",
             response.bytes[7], hci_revision, response.bytes[10], manufacturer, lmp_subversion);

    set_state(S3_STATE_CONTROLLER_READY);
    return ESP_OK;
}

static BaseType_t enqueue_host_to_controller(const radio_h4_packet_t *packet) {
    if (xQueueSend(s_host_to_controller_queue, packet, 0) != pdTRUE) {
        s_diag.host_to_controller_queue_full++;
        request_recovery("host-to-controller queue exhausted");
        return pdFALSE;
    }
    update_high_water(s_host_to_controller_queue, &s_diag.host_to_controller_high_water);
    return pdTRUE;
}

static BaseType_t enqueue_event_to_host(const radio_h4_packet_t *packet) {
    if (xQueueSend(s_event_to_host_queue, packet, 0) != pdTRUE) {
        s_diag.event_to_host_queue_full++;
        request_recovery("event-to-host queue exhausted");
        return pdFALSE;
    }
    update_high_water(s_event_to_host_queue, &s_diag.event_to_host_high_water);
    return pdTRUE;
}

static BaseType_t enqueue_acl_to_host(const radio_h4_packet_t *packet) {
    if (xQueueSend(s_acl_to_host_queue, packet, 0) != pdTRUE) {
        s_diag.acl_to_host_queue_full++;
        request_recovery("ACL-to-host queue exhausted");
        return pdFALSE;
    }
    update_high_water(s_acl_to_host_queue, &s_diag.acl_to_host_high_water);
    return pdTRUE;
}

void radio_usb_bth_hci_command_received_cb(const uint8_t *command, size_t command_len) {
    if (s_state == S3_STATE_RECOVERING || command == NULL ||
        command_len + 1u > sizeof(s_usb_rx_packet.bytes)) {
        request_recovery("invalid USB HCI command callback input");
        return;
    }

    s_usb_rx_packet.bytes[0] = RADIO_H4_TYPE_COMMAND;
    memcpy(&s_usb_rx_packet.bytes[1], command, command_len);
    s_usb_rx_packet.len = command_len + 1u;
    if (radio_h4_validate_complete(s_usb_rx_packet.bytes, s_usb_rx_packet.len) != RADIO_H4_OK) {
        s_diag.malformed_packets++;
        request_recovery("malformed HCI command from USB host");
        return;
    }

    s_diag.usb_commands++;
    (void)enqueue_host_to_controller(&s_usb_rx_packet);
}

void radio_usb_bth_acl_received_cb(const uint8_t *acl, uint16_t acl_len) {
    if (s_state == S3_STATE_RECOVERING || acl == NULL ||
        (size_t)acl_len + 1u > sizeof(s_usb_rx_packet.bytes)) {
        request_recovery("invalid USB ACL callback input");
        return;
    }

    s_usb_rx_packet.bytes[0] = RADIO_H4_TYPE_ACL;
    memcpy(&s_usb_rx_packet.bytes[1], acl, acl_len);
    s_usb_rx_packet.len = (size_t)acl_len + 1u;
    if (radio_h4_validate_complete(s_usb_rx_packet.bytes, s_usb_rx_packet.len) != RADIO_H4_OK) {
        s_diag.malformed_packets++;
        request_recovery("malformed ACL packet from USB host");
        return;
    }

    s_diag.usb_acl_out++;
    (void)enqueue_host_to_controller(&s_usb_rx_packet);
}

void radio_usb_bth_event_sent_cb(uint16_t event_len) {
    (void)event_len;
    if (s_event_usb_task != NULL) {
        xTaskNotifyGive(s_event_usb_task);
    }
}

void radio_usb_bth_acl_sent_cb(uint16_t acl_len) {
    (void)acl_len;
    if (s_acl_usb_task != NULL) {
        xTaskNotifyGive(s_acl_usb_task);
    }
}

void radio_usb_bth_protocol_error_cb(void) {
    s_diag.usb_protocol_errors++;
    request_recovery("USB Bluetooth HCI transport protocol error");
}

static void uart_rx_task(void *arg) {
    (void)arg;
    uint8_t input[S3_UART_READ_CHUNK];
    radio_h4_parser_init(&s_uart_parser);

    for (;;) {
        const int received = uart_read_bytes(S3_HCI_UART, input, sizeof(input), pdMS_TO_TICKS(20));
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
        while (offset < (size_t)received && s_state != S3_STATE_RECOVERING) {
            size_t consumed = 0u;
            const radio_h4_result_t result =
                radio_h4_parser_feed(&s_uart_parser, input + offset, (size_t)received - offset,
                                     &consumed, &s_uart_packet);
            offset += consumed;

            if (result == RADIO_H4_PACKET_READY) {
                switch (s_uart_packet.bytes[0]) {
                case RADIO_H4_TYPE_EVENT:
                    s_diag.uart_events++;
                    (void)enqueue_event_to_host(&s_uart_packet);
                    break;
                case RADIO_H4_TYPE_ACL:
                    s_diag.uart_acl_in++;
                    (void)enqueue_acl_to_host(&s_uart_packet);
                    break;
                case RADIO_H4_TYPE_SCO:
                    s_diag.unsupported_sco++;
                    request_recovery("unexpected SCO/eSCO packet in V1 no-SCO configuration");
                    break;
                default:
                    s_diag.malformed_packets++;
                    request_recovery("unexpected H4 packet direction from WROOM");
                    break;
                }
                continue;
            }
            if (result == RADIO_H4_OK) {
                break;
            }

            s_diag.malformed_packets++;
            request_recovery("malformed H4 stream from WROOM");
            break;
        }
    }
}

static void uart_tx_task(void *arg) {
    (void)arg;
    static radio_h4_packet_t packet;

    for (;;) {
        if (xQueueReceive(s_host_to_controller_queue, &packet, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        if (s_state == S3_STATE_RECOVERING) {
            continue;
        }

        const int written = uart_write_bytes(S3_HCI_UART, packet.bytes, packet.len);
        if (written != (int)packet.len) {
            s_diag.uart_errors++;
            request_recovery("UART write failed or was partial");
            continue;
        }
        if (uart_wait_tx_done(S3_HCI_UART, pdMS_TO_TICKS(S3_CONTROLLER_TX_TIMEOUT_MS)) != ESP_OK) {
            s_diag.uart_errors++;
            request_recovery("UART transmit stalled waiting for WROOM CTS");
        }
    }
}

static void event_usb_task(void *arg) {
    (void)arg;
    static radio_h4_packet_t packet;

    for (;;) {
        if (xQueueReceive(s_event_to_host_queue, &packet, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        while (s_state != S3_STATE_RECOVERING) {
            if (s_state == S3_STATE_OPERATIONAL &&
                radio_usb_bth_event_send(&packet.bytes[1], (uint16_t)(packet.len - 1u))) {
                break;
            }
            (void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(S3_USB_SEND_RETRY_MS));
        }
    }
}

static void acl_usb_task(void *arg) {
    (void)arg;
    static radio_h4_packet_t packet;

    for (;;) {
        if (xQueueReceive(s_acl_to_host_queue, &packet, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        while (s_state != S3_STATE_RECOVERING) {
            if (s_state == S3_STATE_OPERATIONAL &&
                radio_usb_bth_acl_send(&packet.bytes[1], (uint16_t)(packet.len - 1u))) {
                break;
            }
            (void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(S3_USB_SEND_RETRY_MS));
        }
    }
}

static void uart_event_task(void *arg) {
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

static void usb_event_handler(tinyusb_event_t *event, void *arg) {
    (void)arg;
    if (event == NULL) {
        request_recovery("TinyUSB delivered null lifecycle event");
        return;
    }

    switch (event->id) {
    case TINYUSB_EVENT_ATTACHED:
        s_diag.usb_attached++;
        set_state(S3_STATE_OPERATIONAL);
        break;
    case TINYUSB_EVENT_DETACHED:
        s_diag.usb_detached++;
        request_recovery("USB host detached; reset bridge/controller session");
        break;
#ifdef CONFIG_TINYUSB_SUSPEND_CALLBACK
    case TINYUSB_EVENT_SUSPENDED:
        s_diag.usb_suspended++;
        if (s_state == S3_STATE_OPERATIONAL) {
            set_state(S3_STATE_SUSPENDED);
        }
        break;
#endif
#ifdef CONFIG_TINYUSB_RESUME_CALLBACK
    case TINYUSB_EVENT_RESUMED:
        s_diag.usb_resumed++;
        if (s_state == S3_STATE_SUSPENDED) {
            set_state(S3_STATE_OPERATIONAL);
        }
        break;
#endif
    default:
        break;
    }
}

static void diagnostic_task(void *arg) {
    (void)arg;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(S3_DIAGNOSTIC_PERIOD_MS));
        ESP_LOGI(TAG,
                 "diag state=%s usb_cmd=%" PRIu32 " usb_acl_out=%" PRIu32 " uart_evt=%" PRIu32
                 " uart_acl_in=%" PRIu32 " sco=%" PRIu32 " malformed=%" PRIu32 " uart_err=%" PRIu32
                 " qfull=%" PRIu32 "/%" PRIu32 "/%" PRIu32 " qhigh=%u/%u/%u usb=%" PRIu32
                 "/%" PRIu32 "/%" PRIu32 "/%" PRIu32 " usb_proto=%" PRIu32 " recoveries=%" PRIu32,
                 state_name(s_state), s_diag.usb_commands, s_diag.usb_acl_out, s_diag.uart_events,
                 s_diag.uart_acl_in, s_diag.unsupported_sco, s_diag.malformed_packets,
                 s_diag.uart_errors, s_diag.host_to_controller_queue_full,
                 s_diag.event_to_host_queue_full, s_diag.acl_to_host_queue_full,
                 (unsigned)s_diag.host_to_controller_high_water,
                 (unsigned)s_diag.event_to_host_high_water, (unsigned)s_diag.acl_to_host_high_water,
                 s_diag.usb_attached, s_diag.usb_detached, s_diag.usb_suspended, s_diag.usb_resumed,
                 s_diag.usb_protocol_errors, s_diag.recoveries);
    }
}

static void recovery_task(void *arg) {
    (void)arg;
    for (;;) {
        (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        ESP_LOGE(TAG, "controlled recovery: disconnecting USB and restarting S3 bridge MCU");
        if (s_usb_installed) {
            (void)tud_disconnect();
        }
        vTaskDelay(pdMS_TO_TICKS(S3_RECOVERY_DELAY_MS));
        esp_restart();
    }
}

static esp_err_t create_queues_and_recovery_task(void) {
    s_host_to_controller_queue = xQueueCreate(S3_QUEUE_CAPACITY, sizeof(radio_h4_packet_t));
    s_event_to_host_queue = xQueueCreate(S3_QUEUE_CAPACITY, sizeof(radio_h4_packet_t));
    s_acl_to_host_queue = xQueueCreate(S3_QUEUE_CAPACITY, sizeof(radio_h4_packet_t));
    if (s_host_to_controller_queue == NULL || s_event_to_host_queue == NULL ||
        s_acl_to_host_queue == NULL) {
        return ESP_ERR_NO_MEM;
    }

    if (xTaskCreate(recovery_task, "s3_recovery", 3072, NULL, 12, &s_recovery_task) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

static esp_err_t create_transport_tasks(void) {
    if (xTaskCreate(uart_rx_task, "hci_uart_rx", 3072, NULL, 10, NULL) != pdPASS ||
        xTaskCreate(uart_tx_task, "hci_uart_tx", 3072, NULL, 10, NULL) != pdPASS ||
        xTaskCreate(event_usb_task, "hci_usb_evt", 3072, NULL, 10, &s_event_usb_task) != pdPASS ||
        xTaskCreate(acl_usb_task, "hci_usb_acl", 3072, NULL, 10, &s_acl_usb_task) != pdPASS ||
        xTaskCreate(uart_event_task, "hci_uart_evt", 3072, NULL, 11, NULL) != pdPASS ||
        xTaskCreate(diagnostic_task, "hci_diag", 3072, NULL, 4, NULL) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

static esp_err_t install_usb(void) {
    radio_usb_descriptors_init();
    tinyusb_config_t config = TINYUSB_DEFAULT_CONFIG(usb_event_handler);
    config.descriptor.device = radio_usb_device_descriptor();
    config.descriptor.full_speed_config = radio_usb_full_speed_configuration_descriptor();
    config.descriptor.string = radio_usb_string_descriptors();
    config.descriptor.string_count = radio_usb_string_descriptor_count();

    set_state(S3_STATE_USB_ENUMERATING);
    ESP_RETURN_ON_ERROR(tinyusb_driver_install(&config), TAG, "install TinyUSB device stack");
    s_usb_installed = true;
    ESP_LOGI(TAG, "native USB Bluetooth HCI device installed; waiting for host configuration");
    return ESP_OK;
}

esp_err_t s3_bridge_start(void) {
    memset(&s_diag, 0, sizeof(s_diag));
    s_usb_installed = false;
    set_state(S3_STATE_INITIALIZING);

    ESP_RETURN_ON_ERROR(create_queues_and_recovery_task(), TAG,
                        "allocate HCI queues/recovery task");
    ESP_RETURN_ON_ERROR(init_uart(), TAG, "initialize inter-MCU UART");
    ESP_RETURN_ON_ERROR(probe_controller(), TAG, "probe WROOM Bluetooth controller");
    ESP_RETURN_ON_ERROR(create_transport_tasks(), TAG, "create bridge transport tasks");
    ESP_RETURN_ON_ERROR(install_usb(), TAG, "install native USB Bluetooth device");
    return ESP_OK;
}
