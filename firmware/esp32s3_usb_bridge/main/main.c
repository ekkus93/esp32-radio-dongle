#include "esp_idf_version.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "s3_bridge.h"
#include "sdkconfig.h"

#if ESP_IDF_VERSION != ESP_IDF_VERSION_VAL(5, 5, 5)
#error "ESP32 Radio Dongle V1 requires ESP-IDF v5.5.5 exactly"
#endif

#ifndef CONFIG_IDF_TARGET_ESP32S3
#error "esp32s3_usb_bridge must be built for the ESP32-S3 target"
#endif

#define STARTUP_FAILURE_RESTART_DELAY_MS 500

static const char *TAG = "s3_usb_bridge";

void app_main(void)
{
    ESP_LOGI(TAG, "ESP32 Radio Dongle S3 USB bridge starting");
    ESP_LOGI(TAG, "ESP-IDF: %s", esp_get_idf_version());

    const esp_err_t err = s3_bridge_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "bridge startup failed: %s", esp_err_to_name(err));
        vTaskDelay(pdMS_TO_TICKS(STARTUP_FAILURE_RESTART_DELAY_MS));
        esp_restart();
    }
}
