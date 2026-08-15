#include "radio_usb_bth.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "device/usbd.h"
#include "device/usbd_pvt.h"
#include "tusb.h"

extern usbd_class_driver_t const *usbd_app_driver_get_cb(uint8_t *driver_count);

#define EVENT_EP 0x81u
#define ACL_OUT_EP 0x02u
#define ACL_IN_EP 0x82u
#define FS_PACKET_SIZE 64u
#define PRIMARY_DESCRIPTOR_LEN                                                                   \
    (sizeof(tusb_desc_interface_t) + (3u * sizeof(tusb_desc_endpoint_t)))

typedef struct __attribute__((packed)) {
    tusb_desc_interface_t interface;
    tusb_desc_endpoint_t event_in;
    tusb_desc_endpoint_t acl_out;
    tusb_desc_endpoint_t acl_in;
    tusb_desc_interface_t sco_idle;
} descriptor_bundle_t;

static const descriptor_bundle_t DESCRIPTORS = {
    .interface =
        {
            .bLength = sizeof(tusb_desc_interface_t),
            .bDescriptorType = TUSB_DESC_INTERFACE,
            .bInterfaceNumber = 4u,
            .bAlternateSetting = 0u,
            .bNumEndpoints = 3u,
            .bInterfaceClass = TUSB_CLASS_WIRELESS_CONTROLLER,
            .bInterfaceSubClass = 0x01u,
            .bInterfaceProtocol = 0x01u,
            .iInterface = 0u,
        },
    .event_in =
        {
            .bLength = sizeof(tusb_desc_endpoint_t),
            .bDescriptorType = TUSB_DESC_ENDPOINT,
            .bEndpointAddress = EVENT_EP,
            .bmAttributes = {.xfer = TUSB_XFER_INTERRUPT},
            .wMaxPacketSize = 16u,
            .bInterval = 1u,
        },
    .acl_out =
        {
            .bLength = sizeof(tusb_desc_endpoint_t),
            .bDescriptorType = TUSB_DESC_ENDPOINT,
            .bEndpointAddress = ACL_OUT_EP,
            .bmAttributes = {.xfer = TUSB_XFER_BULK},
            .wMaxPacketSize = FS_PACKET_SIZE,
            .bInterval = 0u,
        },
    .acl_in =
        {
            .bLength = sizeof(tusb_desc_endpoint_t),
            .bDescriptorType = TUSB_DESC_ENDPOINT,
            .bEndpointAddress = ACL_IN_EP,
            .bmAttributes = {.xfer = TUSB_XFER_BULK},
            .wMaxPacketSize = FS_PACKET_SIZE,
            .bInterval = 0u,
        },
    .sco_idle =
        {
            .bLength = sizeof(tusb_desc_interface_t),
            .bDescriptorType = TUSB_DESC_INTERFACE,
            .bInterfaceNumber = 5u,
            .bAlternateSetting = 0u,
            .bNumEndpoints = 0u,
            .bInterfaceClass = TUSB_CLASS_WIRELESS_CONTROLLER,
            .bInterfaceSubClass = 0x01u,
            .bInterfaceProtocol = 0x01u,
            .iInterface = 0u,
        },
};

static uint8_t *s_control_buffer;
static uint16_t s_control_length;
static unsigned s_command_count;
static uint8_t s_last_command[16];
static size_t s_last_command_len;

bool tud_control_xfer(uint8_t rhport, const tusb_control_request_t *request, void *buffer,
                      uint16_t length) {
    (void)rhport;
    (void)request;
    s_control_buffer = (uint8_t *)buffer;
    s_control_length = length;
    return true;
}

bool usbd_edpt_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t *buffer, uint16_t total_bytes) {
    (void)rhport;
    (void)ep_addr;
    (void)buffer;
    (void)total_bytes;
    return true;
}

bool usbd_edpt_busy(uint8_t rhport, uint8_t ep_addr) {
    (void)rhport;
    (void)ep_addr;
    return false;
}

bool usbd_edpt_stalled(uint8_t rhport, uint8_t ep_addr) {
    (void)rhport;
    (void)ep_addr;
    return false;
}

bool usbd_edpt_claim(uint8_t rhport, uint8_t ep_addr) {
    (void)rhport;
    (void)ep_addr;
    return true;
}

void usbd_edpt_release(uint8_t rhport, uint8_t ep_addr) {
    (void)rhport;
    (void)ep_addr;
}

bool usbd_edpt_open(uint8_t rhport, const tusb_desc_endpoint_t *endpoint) {
    (void)rhport;
    return endpoint != NULL && endpoint->bDescriptorType == TUSB_DESC_ENDPOINT;
}

bool usbd_open_edpt_pair(uint8_t rhport, const uint8_t *descriptor, uint8_t endpoint_count,
                         uint8_t transfer_type, uint8_t *ep_out, uint8_t *ep_in) {
    (void)rhport;
    if (descriptor == NULL || endpoint_count != 2u || transfer_type != TUSB_XFER_BULK ||
        ep_out == NULL || ep_in == NULL) {
        return false;
    }

    const tusb_desc_endpoint_t *first = (const tusb_desc_endpoint_t *)descriptor;
    const tusb_desc_endpoint_t *second =
        (const tusb_desc_endpoint_t *)((const uint8_t *)first + first->bLength);
    const tusb_desc_endpoint_t *out_ep =
        tu_edpt_dir(first->bEndpointAddress) == TUSB_DIR_OUT ? first : second;
    const tusb_desc_endpoint_t *in_ep = out_ep == first ? second : first;
    if (tu_edpt_dir(out_ep->bEndpointAddress) != TUSB_DIR_OUT ||
        tu_edpt_dir(in_ep->bEndpointAddress) != TUSB_DIR_IN) {
        return false;
    }

    *ep_out = out_ep->bEndpointAddress;
    *ep_in = in_ep->bEndpointAddress;
    return true;
}

void radio_usb_bth_hci_command_received_cb(const uint8_t *command, size_t command_len) {
    assert(command != NULL);
    assert(command_len <= sizeof(s_last_command));
    memcpy(s_last_command, command, command_len);
    s_last_command_len = command_len;
    s_command_count++;
}

static const usbd_class_driver_t *get_driver(void) {
    uint8_t driver_count = 0u;
    const usbd_class_driver_t *driver = usbd_app_driver_get_cb(&driver_count);
    assert(driver != NULL);
    assert(driver_count == 1u);
    return driver;
}

static const usbd_class_driver_t *open_driver(void) {
    const usbd_class_driver_t *driver = get_driver();
    driver->init();
    assert(driver->open(0u, &DESCRIPTORS.interface, sizeof(DESCRIPTORS)) ==
           PRIMARY_DESCRIPTOR_LEN);
    assert(driver->open(0u, &DESCRIPTORS.sco_idle, sizeof(DESCRIPTORS.sco_idle)) ==
           sizeof(tusb_desc_interface_t));
    return driver;
}

static void test_empty_sco_interface_rules(void) {
    const usbd_class_driver_t *driver = get_driver();

    driver->init();
    assert(driver->open(0u, &DESCRIPTORS.sco_idle, sizeof(DESCRIPTORS.sco_idle)) == 0u);

    driver->init();
    assert(driver->open(0u, &DESCRIPTORS.interface, sizeof(DESCRIPTORS)) ==
           PRIMARY_DESCRIPTOR_LEN);

    tusb_desc_interface_t bad = DESCRIPTORS.sco_idle;
    bad.bInterfaceNumber++;
    assert(driver->open(0u, &bad, sizeof(bad)) == 0u);

    bad = DESCRIPTORS.sco_idle;
    bad.bAlternateSetting = 1u;
    assert(driver->open(0u, &bad, sizeof(bad)) == 0u);

    bad = DESCRIPTORS.sco_idle;
    bad.bNumEndpoints = 2u;
    assert(driver->open(0u, &bad, sizeof(bad)) == 0u);

    assert(driver->open(0u, &DESCRIPTORS.sco_idle, sizeof(DESCRIPTORS.sco_idle)) ==
           sizeof(tusb_desc_interface_t));
    assert(driver->open(0u, &DESCRIPTORS.sco_idle, sizeof(DESCRIPTORS.sco_idle)) == 0u);
}

static tusb_control_request_t make_request(uint8_t recipient, uint8_t direction, uint8_t b_request,
                                           uint16_t value, uint16_t index) {
    tusb_control_request_t request = {0};
    request.bmRequestType_bit.direction = direction;
    request.bmRequestType_bit.type = TUSB_REQ_TYPE_CLASS;
    request.bmRequestType_bit.recipient = recipient;
    request.bRequest = b_request;
    request.wValue = value;
    request.wIndex = index;
    request.wLength = 3u;
    return request;
}

static void execute_command(const usbd_class_driver_t *driver, tusb_control_request_t *request) {
    static const uint8_t reset_command[] = {0x03u, 0x0cu, 0x00u};
    assert(driver->control_xfer_cb(0u, CONTROL_STAGE_SETUP, request));
    assert(s_control_buffer != NULL);
    assert(s_control_length == sizeof(reset_command));
    memcpy(s_control_buffer, reset_command, sizeof(reset_command));
    assert(driver->control_xfer_cb(0u, CONTROL_STAGE_DATA, request));
    assert(s_last_command_len == sizeof(reset_command));
    assert(memcmp(s_last_command, reset_command, sizeof(reset_command)) == 0);
}

int main(void) {
    test_empty_sco_interface_rules();
    const usbd_class_driver_t *driver = open_driver();

    tusb_control_request_t request =
        make_request(TUSB_REQ_RCPT_DEVICE, TUSB_DIR_OUT, 0x00u, 0x0000u, 0x0000u);
    execute_command(driver, &request);
    assert(s_command_count == 1u);

    request = make_request(TUSB_REQ_RCPT_DEVICE, TUSB_DIR_OUT, 0xe0u, 0x0000u, 0x0000u);
    execute_command(driver, &request);
    assert(s_command_count == 2u);

    request = make_request(TUSB_REQ_RCPT_DEVICE, TUSB_DIR_OUT, 0x7fu, 0x1234u, 0xbeefu);
    execute_command(driver, &request);
    assert(s_command_count == 3u);

    request = make_request(TUSB_REQ_RCPT_INTERFACE, TUSB_DIR_OUT, 0x00u, 0x0000u,
                           DESCRIPTORS.interface.bInterfaceNumber);
    execute_command(driver, &request);
    assert(s_command_count == 4u);

    request = make_request(TUSB_REQ_RCPT_INTERFACE, TUSB_DIR_OUT, 0xe0u, 0x0000u,
                           DESCRIPTORS.interface.bInterfaceNumber);
    assert(!driver->control_xfer_cb(0u, CONTROL_STAGE_SETUP, &request));

    request = make_request(TUSB_REQ_RCPT_INTERFACE, TUSB_DIR_OUT, 0x00u, 0x0000u,
                           (uint16_t)(DESCRIPTORS.interface.bInterfaceNumber + 1u));
    assert(!driver->control_xfer_cb(0u, CONTROL_STAGE_SETUP, &request));

    request = make_request(TUSB_REQ_RCPT_DEVICE, TUSB_DIR_IN, 0x00u, 0x0000u, 0x0000u);
    assert(!driver->control_xfer_cb(0u, CONTROL_STAGE_SETUP, &request));

    request = make_request(TUSB_REQ_RCPT_DEVICE, TUSB_DIR_OUT, 0x00u, 0x0000u, 0x0000u);
    request.bmRequestType_bit.type = 0u;
    assert(!driver->control_xfer_cb(0u, CONTROL_STAGE_SETUP, &request));

    request = make_request(TUSB_REQ_RCPT_DEVICE, TUSB_DIR_OUT, 0x00u, 0x0000u, 0x0000u);
    request.wLength = 259u;
    assert(!driver->control_xfer_cb(0u, CONTROL_STAGE_SETUP, &request));

    puts("radio_usb_bth legacy control compatibility tests passed");
    return 0;
}
