#include "esp_err.h"
#include "esp_idf_version.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "radio_uart_smoke.h"
#include "sdkconfig.h"

#if ESP_IDF_VERSION != ESP_IDF_VERSION_VAL(5, 5, 5)
#error "ESP32 Radio Dongle V1 requires ESP-IDF v5.5.5 exactly"
#endif

#ifndef CONFIG_IDF_TARGET_ESP32
#error "esp32_wroom_uart_smoke must be built for original ESP32"
#endif

static const char *TAG = "wroom_uart_smoke";

void app_main(void) {
    ESP_LOGI(TAG, "V1-103/V1-104 ESP32-WROOM UART/RTS/CTS bring-up image");

    const esp_err_t err = radio_uart_smoke_run();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "BRINGUP FAIL: %s", esp_err_to_name(err));
    }

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
