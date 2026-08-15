#include "usb_descriptors.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_err.h"
#include "tusb.h"

static esp_err_t s_mac_result = ESP_OK;
static uint8_t s_mac[6] = {0xdeu, 0xadu, 0xbeu, 0xefu, 0x01u, 0x23u};

esp_err_t esp_efuse_mac_get_default(uint8_t mac[6]) {
    if (s_mac_result != ESP_OK) {
        return s_mac_result;
    }
    memcpy(mac, s_mac, sizeof(s_mac));
    return ESP_OK;
}

static uint16_t read_le16(const uint8_t *bytes) {
    return (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8);
}

static void test_device_descriptor(void) {
    const tusb_desc_device_t *descriptor = radio_usb_device_descriptor();
    assert(descriptor != NULL);
    assert(descriptor->bLength == sizeof(tusb_desc_device_t));
    assert(descriptor->bDescriptorType == TUSB_DESC_DEVICE);
    assert(descriptor->bcdUSB == 0x0200u);
    assert(descriptor->bDeviceClass == TUSB_CLASS_WIRELESS_CONTROLLER);
    assert(descriptor->bDeviceSubClass == 0x01u);
    assert(descriptor->bDeviceProtocol == 0x01u);
    assert(descriptor->bMaxPacketSize0 == CFG_TUD_ENDPOINT0_SIZE);
    assert(descriptor->idVendor == 0xcafeu);
    assert(descriptor->idProduct == 0x4011u);
    assert(descriptor->bcdDevice == 0x0100u);
    assert(descriptor->iManufacturer == 1u);
    assert(descriptor->iProduct == 2u);
    assert(descriptor->iSerialNumber == 3u);
    assert(descriptor->bNumConfigurations == 1u);
}

static void test_configuration_descriptor(void) {
    const uint8_t *configuration = radio_usb_full_speed_configuration_descriptor();
    assert(configuration != NULL);

    assert(configuration[0] == 9u);
    assert(configuration[1] == TUSB_DESC_CONFIGURATION);
    assert(read_le16(&configuration[2]) == 39u);
    assert(configuration[4] == 1u);
    assert(configuration[5] == 1u);

    const uint8_t *interface = &configuration[9];
    assert(interface[0] == 9u);
    assert(interface[1] == TUSB_DESC_INTERFACE);
    assert(interface[2] == 0u);
    assert(interface[3] == 0u);
    assert(interface[4] == 3u);
    assert(interface[5] == TUSB_CLASS_WIRELESS_CONTROLLER);
    assert(interface[6] == 0x01u);
    assert(interface[7] == 0x01u);

    const uint8_t *event_in = &configuration[18];
    assert(event_in[0] == 7u);
    assert(event_in[1] == TUSB_DESC_ENDPOINT);
    assert(event_in[2] == 0x81u);
    assert((event_in[3] & 0x03u) == TUSB_XFER_INTERRUPT);
    assert(read_le16(&event_in[4]) == 16u);
    assert(event_in[6] == 1u);

    const uint8_t *acl_out = &configuration[25];
    assert(acl_out[0] == 7u);
    assert(acl_out[1] == TUSB_DESC_ENDPOINT);
    assert(acl_out[2] == 0x02u);
    assert((acl_out[3] & 0x03u) == TUSB_XFER_BULK);
    assert(read_le16(&acl_out[4]) == 64u);

    const uint8_t *acl_in = &configuration[32];
    assert(acl_in[0] == 7u);
    assert(acl_in[1] == TUSB_DESC_ENDPOINT);
    assert(acl_in[2] == 0x82u);
    assert((acl_in[3] & 0x03u) == TUSB_XFER_BULK);
    assert(read_le16(&acl_in[4]) == 64u);
}

static void test_string_descriptors_and_stable_serial(void) {
    s_mac_result = ESP_OK;
    const uint8_t first_mac[6] = {0xdeu, 0xadu, 0xbeu, 0xefu, 0x01u, 0x23u};
    memcpy(s_mac, first_mac, sizeof(s_mac));
    radio_usb_descriptors_init();

    assert(radio_usb_string_descriptor_count() == 4);
    const char **strings = radio_usb_string_descriptors();
    assert(strings != NULL);
    assert((uint8_t)strings[0][0] == 0x09u);
    assert((uint8_t)strings[0][1] == 0x04u);
    assert(strcmp(strings[1], "ESP32 Radio Dongle") == 0);
    assert(strcmp(strings[2], "ESP32 Radio Dongle V1") == 0);
    assert(strcmp(strings[3], "DEADBEEF0123") == 0);

    radio_usb_descriptors_init();
    strings = radio_usb_string_descriptors();
    assert(strcmp(strings[3], "DEADBEEF0123") == 0);

    const uint8_t second_mac[6] = {0x02u, 0x00u, 0x00u, 0x00u, 0x00u, 0x01u};
    memcpy(s_mac, second_mac, sizeof(s_mac));
    radio_usb_descriptors_init();
    strings = radio_usb_string_descriptors();
    assert(strcmp(strings[3], "020000000001") == 0);
}

static void test_serial_fallback_is_explicit(void) {
    s_mac_result = ESP_FAIL;
    radio_usb_descriptors_init();
    const char **strings = radio_usb_string_descriptors();
    assert(strcmp(strings[3], "S3UNKNOWN") == 0);
    s_mac_result = ESP_OK;
}

int main(void) {
    test_device_descriptor();
    test_configuration_descriptor();
    test_string_descriptors_and_stable_serial();
    test_serial_fallback_is_explicit();
    puts("usb_descriptors host tests passed");
    return 0;
}
