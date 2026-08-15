#ifndef TEST_STUB_ESP_MAC_H
#define TEST_STUB_ESP_MAC_H

#include <stdint.h>

#include "esp_err.h"

esp_err_t esp_efuse_mac_get_default(uint8_t mac[6]);

#endif
