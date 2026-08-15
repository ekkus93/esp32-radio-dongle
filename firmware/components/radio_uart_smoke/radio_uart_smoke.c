#include "radio_uart_smoke.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "driver/uart.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "radio_link_config.h"
#include "sdkconfig.h"

#if defined(CONFIG_IDF_TARGET_ESP32S3)
#define SMOKE_UART UART_NUM_1
#define SMOKE_TX_GPIO 4
#define SMOKE_RX_GPIO 5
#define SMOKE_RTS_GPIO 6
#define SMOKE_CTS_GPIO 7
#define SMOKE_ROLE_INITIATOR 1
#define SMOKE_ROLE_NAME "S3 initiator"
#elif defined(CONFIG_IDF_TARGET_ESP32)
#define SMOKE_UART UART_NUM_2
#define SMOKE_TX_GPIO 17
#define SMOKE_RX_GPIO 16
#define SMOKE_RTS_GPIO 26
#define SMOKE_CTS_GPIO 25
#define SMOKE_ROLE_INITIATOR 0
#define SMOKE_ROLE_NAME "WROOM responder"
#else
#error "radio_uart_smoke supports only ESP32-S3 and original ESP32 targets"
#endif

#define SMOKE_UART_RX_BUFFER_SIZE 2048
#define SMOKE_UART_TX_BUFFER_SIZE 2048
#define SMOKE_UART_RX_FLOW_THRESHOLD 96
#define SMOKE_IO_TIMEOUT_MS 2000
#define SMOKE_SYNC_IO_TIMEOUT_MS 500
#define SMOKE_PEER_SYNC_ATTEMPTS 30
#define SMOKE_PEER_SYNC_DELAY_MS 100
#define SMOKE_UART_RECOVERY_DELAY_MS 50
#define SMOKE_ROUND_DELAY_MS 2000
#define SMOKE_PING_COUNT 32
#define SMOKE_FLOW_PAYLOAD_SIZE 1024
#define SMOKE_FLOW_STALL_MS 600
#define SMOKE_REVERSE_START_DELAY_MS 100
#define SMOKE_MIN_EXPECTED_BLOCK_MS 350

static const char *TAG = "uart_smoke";

static const uint8_t CMD_FLOW_A[4] = {'F', 'W', 'A', '1'};
static const uint8_t CMD_FLOW_B[4] = {'F', 'W', 'B', '1'};
static const uint8_t ACK_FLOW_A[4] = {'R', 'D', 'Y', '1'};
static const uint8_t OK_FLOW_A[4] = {'O', 'K', 'A', '1'};
static const uint8_t BAD_FLOW_A[4] = {'B', 'A', 'D', 'A'};
#if SMOKE_ROLE_INITIATOR
static const uint8_t OK_FLOW_B[4] = {'O', 'K', 'B', '1'};
#else
static const uint8_t CMD_PING[4] = {'P', 'I', 'N', 'G'};
#endif

static uint8_t payload_byte(size_t offset) { return (uint8_t)(((offset * 37u) + 0x5au) & 0xffu); }

static void fill_payload(uint8_t *buffer, size_t length) {
    for (size_t i = 0; i < length; ++i) {
        buffer[i] = payload_byte(i);
    }
}

static bool payload_matches(const uint8_t *buffer, size_t length) {
    for (size_t i = 0; i < length; ++i) {
        if (buffer[i] != payload_byte(i)) {
            ESP_LOGE(TAG, "payload mismatch at offset %u: got=0x%02x expected=0x%02x", (unsigned)i,
                     buffer[i], payload_byte(i));
            return false;
        }
    }
    return true;
}

static esp_err_t read_exact(uint8_t *buffer, size_t length, uint32_t timeout_ms) {
    const int64_t deadline_us = esp_timer_get_time() + ((int64_t)timeout_ms * 1000);
    size_t offset = 0;

    while (offset < length) {
        if (esp_timer_get_time() >= deadline_us) {
            return ESP_ERR_TIMEOUT;
        }

        const int received =
            uart_read_bytes(SMOKE_UART, buffer + offset, length - offset, pdMS_TO_TICKS(20));
        if (received < 0) {
            return ESP_FAIL;
        }
        offset += (size_t)received;
    }

    return ESP_OK;
}

static esp_err_t write_and_drain(const uint8_t *buffer, size_t length, uint32_t timeout_ms) {
    const int written = uart_write_bytes(SMOKE_UART, buffer, length);
    if (written != (int)length) {
        ESP_LOGE(TAG, "UART write accepted %d/%u bytes", written, (unsigned)length);
        return ESP_FAIL;
    }
    return uart_wait_tx_done(SMOKE_UART, pdMS_TO_TICKS(timeout_ms));
}

static esp_err_t init_uart(void) {
    const uart_config_t config = {
        .baud_rate = RADIO_HCI_UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_CTS_RTS,
        .rx_flow_ctrl_thresh = SMOKE_UART_RX_FLOW_THRESHOLD,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_RETURN_ON_ERROR(uart_driver_install(SMOKE_UART, SMOKE_UART_RX_BUFFER_SIZE,
                                            SMOKE_UART_TX_BUFFER_SIZE, 0, NULL, 0),
                        TAG, "install smoke-test UART driver");
    ESP_RETURN_ON_ERROR(uart_param_config(SMOKE_UART, &config), TAG, "configure smoke-test UART");
    ESP_RETURN_ON_ERROR(
        uart_set_pin(SMOKE_UART, SMOKE_TX_GPIO, SMOKE_RX_GPIO, SMOKE_RTS_GPIO, SMOKE_CTS_GPIO), TAG,
        "route smoke-test UART pins");
    ESP_RETURN_ON_ERROR(uart_flush_input(SMOKE_UART), TAG, "flush smoke-test UART input");

    uart_hw_flowcontrol_t flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    ESP_RETURN_ON_ERROR(uart_get_hw_flow_ctrl(SMOKE_UART, &flow_ctrl), TAG,
                        "read back smoke-test UART flow-control mode");
    if (flow_ctrl != UART_HW_FLOWCTRL_CTS_RTS) {
        ESP_LOGE(TAG, "UART hardware flow-control readback mismatch: got=%d expected=%d",
                 (int)flow_ctrl, (int)UART_HW_FLOWCTRL_CTS_RTS);
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "%s: UART=%d baud=%d TX=%d RX=%d RTS=%d CTS=%d flow=CTS_RTS threshold=%u",
             SMOKE_ROLE_NAME, (int)SMOKE_UART, RADIO_HCI_UART_BAUD, SMOKE_TX_GPIO, SMOKE_RX_GPIO,
             SMOKE_RTS_GPIO, SMOKE_CTS_GPIO, SMOKE_UART_RX_FLOW_THRESHOLD);
    return ESP_OK;
}

static esp_err_t recover_uart(void) {
    ESP_RETURN_ON_ERROR(uart_driver_delete(SMOKE_UART), TAG, "delete smoke-test UART driver");
    vTaskDelay(pdMS_TO_TICKS(SMOKE_UART_RECOVERY_DELAY_MS));
    return init_uart();
}

#if SMOKE_ROLE_INITIATOR

static esp_err_t ping_once(uint32_t sequence, uint32_t timeout_ms) {
    uint8_t request[8] = {'P', 'I', 'N', 'G', 0, 0, 0, 0};
    uint8_t response[sizeof(request)] = {0};
    request[4] = (uint8_t)(sequence & 0xffu);
    request[5] = (uint8_t)((sequence >> 8) & 0xffu);
    request[6] = (uint8_t)((sequence >> 16) & 0xffu);
    request[7] = (uint8_t)((sequence >> 24) & 0xffu);

    ESP_RETURN_ON_ERROR(write_and_drain(request, sizeof(request), timeout_ms), TAG, "send ping");
    ESP_RETURN_ON_ERROR(read_exact(response, sizeof(response), timeout_ms), TAG,
                        "receive ping echo");
    if (memcmp(request, response, sizeof(request)) != 0) {
        ESP_LOGE(TAG, "ping echo mismatch at sequence %" PRIu32, sequence);
        return ESP_ERR_INVALID_RESPONSE;
    }
    return ESP_OK;
}

static esp_err_t synchronize_peer(void) {
    for (unsigned attempt = 1; attempt <= SMOKE_PEER_SYNC_ATTEMPTS; ++attempt) {
        const esp_err_t ping_err = ping_once(0u, SMOKE_SYNC_IO_TIMEOUT_MS);
        if (ping_err == ESP_OK) {
            ESP_LOGI(TAG, "peer synchronized after %u attempt(s)", attempt);
            return ESP_OK;
        }

        ESP_LOGW(TAG, "peer sync attempt %u/%u failed: %s; resetting local UART state", attempt,
                 SMOKE_PEER_SYNC_ATTEMPTS, esp_err_to_name(ping_err));
        ESP_RETURN_ON_ERROR(recover_uart(), TAG, "recover UART after failed peer sync");
        vTaskDelay(pdMS_TO_TICKS(SMOKE_PEER_SYNC_DELAY_MS));
    }

    ESP_LOGE(TAG, "could not synchronize with WROOM responder");
    return ESP_ERR_TIMEOUT;
}

static esp_err_t run_ping_test(void) {
    for (uint32_t sequence = 1; sequence <= SMOKE_PING_COUNT; ++sequence) {
        ESP_RETURN_ON_ERROR(ping_once(sequence, SMOKE_IO_TIMEOUT_MS), TAG,
                            "bidirectional ping/echo failed");
    }
    ESP_LOGI(TAG, "PASS: %u bidirectional ping/echo frames", SMOKE_PING_COUNT);
    return ESP_OK;
}

static esp_err_t run_s3_to_wroom_flow_test(void) {
    uint8_t payload[SMOKE_FLOW_PAYLOAD_SIZE];
    uint8_t ack[sizeof(ACK_FLOW_A)] = {0};
    uint8_t result[sizeof(OK_FLOW_A)] = {0};
    fill_payload(payload, sizeof(payload));

    ESP_RETURN_ON_ERROR(write_and_drain(CMD_FLOW_A, sizeof(CMD_FLOW_A), SMOKE_IO_TIMEOUT_MS), TAG,
                        "send forward-flow command");
    ESP_RETURN_ON_ERROR(read_exact(ack, sizeof(ack), SMOKE_IO_TIMEOUT_MS), TAG,
                        "wait for forward-flow ready acknowledgement");
    if (memcmp(ack, ACK_FLOW_A, sizeof(ack)) != 0) {
        ESP_LOGE(TAG, "unexpected forward-flow acknowledgement");
        return ESP_ERR_INVALID_RESPONSE;
    }

    const int64_t started_us = esp_timer_get_time();
    ESP_RETURN_ON_ERROR(write_and_drain(payload, sizeof(payload), SMOKE_IO_TIMEOUT_MS), TAG,
                        "forward-flow payload did not drain through CTS");
    const uint32_t elapsed_ms = (uint32_t)((esp_timer_get_time() - started_us) / 1000);

    ESP_RETURN_ON_ERROR(read_exact(result, sizeof(result), SMOKE_IO_TIMEOUT_MS), TAG,
                        "wait for forward-flow validation result");
    if (memcmp(result, BAD_FLOW_A, sizeof(result)) == 0) {
        ESP_LOGE(TAG, "WROOM reported corrupted forward-flow payload");
        return ESP_ERR_INVALID_CRC;
    }
    if (memcmp(result, OK_FLOW_A, sizeof(result)) != 0) {
        ESP_LOGE(TAG, "unexpected forward-flow validation result");
        return ESP_ERR_INVALID_RESPONSE;
    }
    if (elapsed_ms < SMOKE_MIN_EXPECTED_BLOCK_MS) {
        ESP_LOGE(TAG,
                 "forward flow did not show expected CTS backpressure: elapsed=%" PRIu32
                 " ms, expected >= %u ms",
                 elapsed_ms, SMOKE_MIN_EXPECTED_BLOCK_MS);
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG,
             "PASS: WROOM RTS -> S3 CTS backpressure asserted/released; %u-byte payload drained in "
             "%" PRIu32 " ms",
             SMOKE_FLOW_PAYLOAD_SIZE, elapsed_ms);
    return ESP_OK;
}

static esp_err_t run_wroom_to_s3_flow_test(void) {
    uint8_t payload[SMOKE_FLOW_PAYLOAD_SIZE] = {0};
    uint8_t result[8] = {0};

    ESP_RETURN_ON_ERROR(write_and_drain(CMD_FLOW_B, sizeof(CMD_FLOW_B), SMOKE_IO_TIMEOUT_MS), TAG,
                        "send reverse-flow command");

    ESP_RETURN_ON_ERROR(uart_disable_rx_intr(SMOKE_UART), TAG,
                        "disable S3 RX interrupt for reverse-flow stall");
    vTaskDelay(pdMS_TO_TICKS(SMOKE_FLOW_STALL_MS));
    ESP_RETURN_ON_ERROR(uart_enable_rx_intr(SMOKE_UART), TAG,
                        "re-enable S3 RX interrupt after reverse-flow stall");

    ESP_RETURN_ON_ERROR(read_exact(payload, sizeof(payload), SMOKE_IO_TIMEOUT_MS), TAG,
                        "receive reverse-flow payload");
    if (!payload_matches(payload, sizeof(payload))) {
        return ESP_ERR_INVALID_CRC;
    }

    ESP_RETURN_ON_ERROR(read_exact(result, sizeof(result), SMOKE_IO_TIMEOUT_MS), TAG,
                        "receive reverse-flow timing result");
    if (memcmp(result, OK_FLOW_B, sizeof(OK_FLOW_B)) != 0) {
        ESP_LOGE(TAG, "unexpected reverse-flow validation result");
        return ESP_ERR_INVALID_RESPONSE;
    }

    const uint32_t elapsed_ms = (uint32_t)result[4] | ((uint32_t)result[5] << 8) |
                                ((uint32_t)result[6] << 16) | ((uint32_t)result[7] << 24);
    if (elapsed_ms < SMOKE_MIN_EXPECTED_BLOCK_MS) {
        ESP_LOGE(TAG,
                 "reverse flow did not show expected CTS backpressure: elapsed=%" PRIu32
                 " ms, expected >= %u ms",
                 elapsed_ms, SMOKE_MIN_EXPECTED_BLOCK_MS);
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG,
             "PASS: S3 RTS -> WROOM CTS backpressure asserted/released; %u-byte payload drained in "
             "%" PRIu32 " ms",
             SMOKE_FLOW_PAYLOAD_SIZE, elapsed_ms);
    return ESP_OK;
}

static esp_err_t run_initiator_round(uint32_t round) {
    ESP_LOGI(TAG, "ROUND %" PRIu32 ": synchronizing peer", round);
    ESP_RETURN_ON_ERROR(synchronize_peer(), TAG, "peer synchronization failed");
    ESP_RETURN_ON_ERROR(run_ping_test(), TAG, "ping/echo test failed");
    ESP_RETURN_ON_ERROR(run_s3_to_wroom_flow_test(), TAG, "S3-to-WROOM flow-control test failed");
    ESP_RETURN_ON_ERROR(run_wroom_to_s3_flow_test(), TAG, "WROOM-to-S3 flow-control test failed");

    ESP_LOGI(TAG,
             "ROUND %" PRIu32
             " PASS: TX/RX and both RTS/CTS crossings asserted, throttled, released, and resumed",
             round);
    return ESP_OK;
}

static esp_err_t run_initiator(void) {
    uint32_t round = 1u;

    for (;;) {
        const esp_err_t round_err = run_initiator_round(round);
        if (round_err == ESP_OK) {
            ESP_LOGI(TAG, "BRINGUP PASS: round=%" PRIu32, round);
            if (round == 1u) {
                ESP_LOGI(
                    TAG,
                    "RESET TEST READY: keep both boards powered; reset either MCU and require a "
                    "later round PASS");
            }
            round++;
            vTaskDelay(pdMS_TO_TICKS(SMOKE_ROUND_DELAY_MS));
            continue;
        }

        ESP_LOGW(TAG, "ROUND %" PRIu32 " failed: %s; reinitializing UART and re-synchronizing",
                 round, esp_err_to_name(round_err));
        ESP_RETURN_ON_ERROR(recover_uart(), TAG, "recover UART after failed smoke-test round");
        vTaskDelay(pdMS_TO_TICKS(SMOKE_PEER_SYNC_DELAY_MS));
    }
}

#else

static esp_err_t responder_ping(void) {
    uint8_t sequence[4] = {0};
    uint8_t response[8] = {'P', 'I', 'N', 'G', 0, 0, 0, 0};
    ESP_RETURN_ON_ERROR(read_exact(sequence, sizeof(sequence), SMOKE_IO_TIMEOUT_MS), TAG,
                        "read ping sequence");
    memcpy(response + 4, sequence, sizeof(sequence));
    return write_and_drain(response, sizeof(response), SMOKE_IO_TIMEOUT_MS);
}

static esp_err_t responder_flow_a(void) {
    uint8_t payload[SMOKE_FLOW_PAYLOAD_SIZE] = {0};

    ESP_RETURN_ON_ERROR(write_and_drain(ACK_FLOW_A, sizeof(ACK_FLOW_A), SMOKE_IO_TIMEOUT_MS), TAG,
                        "send forward-flow ready acknowledgement");
    ESP_RETURN_ON_ERROR(uart_disable_rx_intr(SMOKE_UART), TAG,
                        "disable WROOM RX interrupt for forward-flow stall");
    vTaskDelay(pdMS_TO_TICKS(SMOKE_FLOW_STALL_MS));
    ESP_RETURN_ON_ERROR(uart_enable_rx_intr(SMOKE_UART), TAG,
                        "re-enable WROOM RX interrupt after forward-flow stall");

    ESP_RETURN_ON_ERROR(read_exact(payload, sizeof(payload), SMOKE_IO_TIMEOUT_MS), TAG,
                        "receive forward-flow payload");
    if (!payload_matches(payload, sizeof(payload))) {
        (void)write_and_drain(BAD_FLOW_A, sizeof(BAD_FLOW_A), SMOKE_IO_TIMEOUT_MS);
        return ESP_ERR_INVALID_CRC;
    }

    ESP_RETURN_ON_ERROR(write_and_drain(OK_FLOW_A, sizeof(OK_FLOW_A), SMOKE_IO_TIMEOUT_MS), TAG,
                        "send forward-flow validation result");
    ESP_LOGI(TAG, "PASS: received intact S3-to-WROOM flow-control payload after RTS release");
    return ESP_OK;
}

static esp_err_t responder_flow_b(void) {
    uint8_t payload[SMOKE_FLOW_PAYLOAD_SIZE];
    uint8_t result[8] = {'O', 'K', 'B', '1', 0, 0, 0, 0};
    fill_payload(payload, sizeof(payload));

    vTaskDelay(pdMS_TO_TICKS(SMOKE_REVERSE_START_DELAY_MS));
    const int64_t started_us = esp_timer_get_time();
    ESP_RETURN_ON_ERROR(write_and_drain(payload, sizeof(payload), SMOKE_IO_TIMEOUT_MS), TAG,
                        "reverse-flow payload did not drain through CTS");
    const uint32_t elapsed_ms = (uint32_t)((esp_timer_get_time() - started_us) / 1000);

    result[4] = (uint8_t)(elapsed_ms & 0xffu);
    result[5] = (uint8_t)((elapsed_ms >> 8) & 0xffu);
    result[6] = (uint8_t)((elapsed_ms >> 16) & 0xffu);
    result[7] = (uint8_t)((elapsed_ms >> 24) & 0xffu);
    ESP_RETURN_ON_ERROR(write_and_drain(result, sizeof(result), SMOKE_IO_TIMEOUT_MS), TAG,
                        "send reverse-flow timing result");

    ESP_LOGI(TAG, "reverse-flow payload drained in %" PRIu32 " ms", elapsed_ms);
    return ESP_OK;
}

static esp_err_t run_responder(void) {
    ESP_LOGI(TAG, "READY: waiting for ESP32-S3 smoke-test initiator");

    for (;;) {
        uint8_t command[4] = {0};
        const esp_err_t read_err = read_exact(command, sizeof(command), 60000);
        if (read_err == ESP_ERR_TIMEOUT) {
            ESP_LOGI(TAG, "READY: still waiting for ESP32-S3 smoke-test initiator");
            continue;
        }
        if (read_err != ESP_OK) {
            ESP_LOGW(TAG, "responder command read failed: %s; resetting UART state",
                     esp_err_to_name(read_err));
            ESP_RETURN_ON_ERROR(recover_uart(), TAG, "recover responder UART after command read");
            continue;
        }

        esp_err_t phase_err = ESP_OK;
        if (memcmp(command, CMD_PING, sizeof(command)) == 0) {
            phase_err = responder_ping();
        } else if (memcmp(command, CMD_FLOW_A, sizeof(command)) == 0) {
            phase_err = responder_flow_a();
        } else if (memcmp(command, CMD_FLOW_B, sizeof(command)) == 0) {
            phase_err = responder_flow_b();
        } else {
            ESP_LOGW(TAG,
                     "unknown/unaligned smoke-test command: %02x %02x %02x %02x; resetting UART "
                     "state",
                     command[0], command[1], command[2], command[3]);
            ESP_RETURN_ON_ERROR(recover_uart(), TAG,
                                "recover responder UART after unknown command");
            continue;
        }

        if (phase_err != ESP_OK) {
            ESP_LOGW(TAG, "responder phase failed: %s; resetting UART state for peer re-sync",
                     esp_err_to_name(phase_err));
            ESP_RETURN_ON_ERROR(recover_uart(), TAG, "recover responder UART after phase failure");
        }
    }
}

#endif

esp_err_t radio_uart_smoke_run(void) {
    ESP_RETURN_ON_ERROR(init_uart(), TAG, "initialize smoke-test UART");

#if SMOKE_ROLE_INITIATOR
    return run_initiator();
#else
    return run_responder();
#endif
}
