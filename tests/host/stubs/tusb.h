#ifndef TEST_STUB_TUSB_H
#define TEST_STUB_TUSB_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TU_ATTR_WEAK __attribute__((weak))
#define CFG_TUD_MEM_SECTION
#define TUD_EPBUF_DEF(_name, _size) uint8_t _name[_size]

#define CFG_TUD_ENDPOINT0_SIZE 64u
#define TUD_CONFIG_DESC_LEN 9u

#define TUSB_CLASS_WIRELESS_CONTROLLER 0xe0u
#define TUSB_DESC_DEVICE 0x01u
#define TUSB_DESC_CONFIGURATION 0x02u
#define TUSB_DESC_INTERFACE 0x04u
#define TUSB_DESC_ENDPOINT 0x05u
#define TUSB_XFER_BULK 0x02u
#define TUSB_XFER_INTERRUPT 0x03u
#define TUSB_DIR_OUT 0u
#define TUSB_DIR_IN 1u
#define TUSB_REQ_TYPE_CLASS 1u
#define TUSB_REQ_RCPT_DEVICE 0u
#define TUSB_REQ_RCPT_INTERFACE 1u

#define CONTROL_STAGE_SETUP 0u
#define CONTROL_STAGE_DATA 1u
#define CONTROL_STAGE_ACK 2u

#define TUD_CONFIG_DESCRIPTOR(_config_num, _itf_count, _stridx, _total_len, _attribute, _power_ma) \
    9u, TUSB_DESC_CONFIGURATION, (uint8_t)((_total_len) & 0xffu),                            \
        (uint8_t)(((_total_len) >> 8) & 0xffu), (uint8_t)(_itf_count),                       \
        (uint8_t)(_config_num), (uint8_t)(_stridx), (uint8_t)(0x80u | (_attribute)),         \
        (uint8_t)((_power_ma) / 2u)

typedef enum {
    XFER_RESULT_SUCCESS = 0,
    XFER_RESULT_FAILED = 1,
} xfer_result_t;

typedef struct __attribute__((packed)) {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t bcdUSB;
    uint8_t bDeviceClass;
    uint8_t bDeviceSubClass;
    uint8_t bDeviceProtocol;
    uint8_t bMaxPacketSize0;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t iManufacturer;
    uint8_t iProduct;
    uint8_t iSerialNumber;
    uint8_t bNumConfigurations;
} tusb_desc_device_t;

typedef struct __attribute__((packed)) {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bInterfaceNumber;
    uint8_t bAlternateSetting;
    uint8_t bNumEndpoints;
    uint8_t bInterfaceClass;
    uint8_t bInterfaceSubClass;
    uint8_t bInterfaceProtocol;
    uint8_t iInterface;
} tusb_desc_interface_t;

typedef struct __attribute__((packed)) {
    uint8_t xfer : 2;
    uint8_t sync : 2;
    uint8_t usage : 2;
    uint8_t reserved : 2;
} tusb_ep_attr_t;

typedef struct __attribute__((packed)) {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bEndpointAddress;
    tusb_ep_attr_t bmAttributes;
    uint16_t wMaxPacketSize;
    uint8_t bInterval;
} tusb_desc_endpoint_t;

typedef struct {
    uint8_t recipient : 5;
    uint8_t type : 2;
    uint8_t direction : 1;
} tusb_request_type_bits_t;

typedef struct {
    tusb_request_type_bits_t bmRequestType_bit;
    uint8_t bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} tusb_control_request_t;

static inline const void *tu_desc_next(const void *descriptor) {
    const uint8_t *bytes = (const uint8_t *)descriptor;
    return bytes + bytes[0];
}

static inline uint8_t tu_edpt_dir(uint8_t endpoint_address) {
    return (endpoint_address & 0x80u) != 0u ? TUSB_DIR_IN : TUSB_DIR_OUT;
}

static inline uint16_t tu_edpt_packet_size(const tusb_desc_endpoint_t *endpoint) {
    return endpoint->wMaxPacketSize;
}

bool tud_control_xfer(uint8_t rhport, const tusb_control_request_t *request, void *buffer,
                      uint16_t length);

#endif
