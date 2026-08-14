#include "esp_idf_version.h"
#include "esp_log.h"
#include "sdkconfig.h"

#if ESP_IDF_VERSION != ESP_IDF_VERSION_VAL(5, 5, 5)
#error "ESP32 Radio Dongle V1 requires ESP-IDF v5.5.5 exactly"
#endif

#ifndef CONFIG_IDF_TARGET_ESP32S3
#error "esp32s3_usb_bridge must be built for the ESP32-S3 target"
#endif

static const char *TAG = "s3_usb_bridge";

void app_main(void)
{
    ESP_LOGI(TAG, "ESP32 Radio Dongle S3 USB bridge baseline started");
    ESP_LOGI(TAG, "ESP-IDF: %s", esp_get_idf_version());
}
