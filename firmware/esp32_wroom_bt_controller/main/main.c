#include "esp_idf_version.h"
#include "esp_log.h"
#include "sdkconfig.h"

#if ESP_IDF_VERSION != ESP_IDF_VERSION_VAL(5, 5, 5)
#error "ESP32 Radio Dongle V1 requires ESP-IDF v5.5.5 exactly"
#endif

#ifndef CONFIG_IDF_TARGET_ESP32
#error "esp32_wroom_bt_controller must be built for the original ESP32 target"
#endif

static const char *TAG = "wroom_bt_ctrl";

void app_main(void)
{
    ESP_LOGI(TAG, "ESP32 Radio Dongle WROOM controller baseline started");
    ESP_LOGI(TAG, "ESP-IDF: %s", esp_get_idf_version());
}
