#include "usb_descriptors.h"

#include <stdio.h>

#include "esp_err.h"
#include "esp_mac.h"

#define RADIO_USB_DEV_VID 0xCAFEu
#define RADIO_USB_DEV_PID 0x4011u
#define RADIO_USB_BCD 0x0200u
#define RADIO_USB_DEVICE_VERSION 0x0100u

#define RADIO_USB_INTERFACE_NUMBER 0u
#define RADIO_USB_INTERFACE_COUNT 1u

#define RADIO_USB_EP_EVENT_IN 0x81u
#define RADIO_USB_EP_ACL_OUT 0x02u
#define RADIO_USB_EP_ACL_IN 0x82u
#define RADIO_USB_EVENT_EP_SIZE 16u
#define RADIO_USB_ACL_EP_SIZE 64u
#define RADIO_USB_EVENT_INTERVAL 1u

#define RADIO_USB_CONFIGURATION_TOTAL_LEN                                                          \
    (TUD_CONFIG_DESC_LEN + sizeof(tusb_desc_interface_t) + (3u * sizeof(tusb_desc_endpoint_t)))

#define RADIO_USB_MANUFACTURER_INDEX 1u
#define RADIO_USB_PRODUCT_INDEX 2u
#define RADIO_USB_SERIAL_INDEX 3u

#define RADIO_USB_INTERFACE_DESCRIPTOR(_itf, _alt, _num_ep, _class, _subclass, _protocol, _str)    \
    9, TUSB_DESC_INTERFACE, (_itf), (_alt), (_num_ep), (_class), (_subclass), (_protocol), (_str)

#define RADIO_USB_ENDPOINT_DESCRIPTOR(_addr, _attr, _packet_size, _interval)                         \
    7, TUSB_DESC_ENDPOINT, (_addr), (_attr), (uint8_t)((_packet_size) & 0xffu),                      \
        (uint8_t)(((_packet_size) >> 8) & 0xffu), (_interval)

static const tusb_desc_device_t s_device_descriptor = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = RADIO_USB_BCD,
    .bDeviceClass = TUSB_CLASS_WIRELESS_CONTROLLER,
    .bDeviceSubClass = 0x01,
    .bDeviceProtocol = 0x01,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = RADIO_USB_DEV_VID,
    .idProduct = RADIO_USB_DEV_PID,
    .bcdDevice = RADIO_USB_DEVICE_VERSION,
    .iManufacturer = RADIO_USB_MANUFACTURER_INDEX,
    .iProduct = RADIO_USB_PRODUCT_INDEX,
    .iSerialNumber = RADIO_USB_SERIAL_INDEX,
    .bNumConfigurations = 1,
};

static const uint8_t s_full_speed_configuration[] = {
    TUD_CONFIG_DESCRIPTOR(1, RADIO_USB_INTERFACE_COUNT, 0, RADIO_USB_CONFIGURATION_TOTAL_LEN, 0,
                          500),

    RADIO_USB_INTERFACE_DESCRIPTOR(RADIO_USB_INTERFACE_NUMBER, 0, 3,
                                   TUSB_CLASS_WIRELESS_CONTROLLER, 0x01, 0x01, 0),

    RADIO_USB_ENDPOINT_DESCRIPTOR(RADIO_USB_EP_EVENT_IN, TUSB_XFER_INTERRUPT,
                                  RADIO_USB_EVENT_EP_SIZE, RADIO_USB_EVENT_INTERVAL),
    RADIO_USB_ENDPOINT_DESCRIPTOR(RADIO_USB_EP_ACL_OUT, TUSB_XFER_BULK, RADIO_USB_ACL_EP_SIZE, 0),
    RADIO_USB_ENDPOINT_DESCRIPTOR(RADIO_USB_EP_ACL_IN, TUSB_XFER_BULK, RADIO_USB_ACL_EP_SIZE, 0),
};

static const char s_language_id[] = {0x09, 0x04};
static char s_serial_number[16] = "S3UNSET";
static const char *s_string_descriptors[] = {
    s_language_id,
    "ESP32 Radio Dongle",
    "ESP32 Radio Dongle V1",
    s_serial_number,
};

_Static_assert(sizeof(s_full_speed_configuration) == RADIO_USB_CONFIGURATION_TOTAL_LEN,
               "USB configuration descriptor length mismatch");

void radio_usb_descriptors_init(void) {
    uint8_t mac[6] = {0};
    if (esp_efuse_mac_get_default(mac) != ESP_OK) {
        (void)snprintf(s_serial_number, sizeof(s_serial_number), "S3UNKNOWN");
        return;
    }

    (void)snprintf(s_serial_number, sizeof(s_serial_number), "%02X%02X%02X%02X%02X%02X", mac[0],
                   mac[1], mac[2], mac[3], mac[4], mac[5]);
}

const tusb_desc_device_t *radio_usb_device_descriptor(void) { return &s_device_descriptor; }

const uint8_t *radio_usb_full_speed_configuration_descriptor(void) {
    return s_full_speed_configuration;
}

const char **radio_usb_string_descriptors(void) { return s_string_descriptors; }

int radio_usb_string_descriptor_count(void) {
    return (int)(sizeof(s_string_descriptors) / sizeof(s_string_descriptors[0]));
}
