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

typedef struct __attribute__((packed)) {
    tusb_desc_interface_t interface;
    tusb_desc_endpoint_t event_in;
    tusb_desc_endpoint_t acl_out;
    tusb_desc_endpoint_t acl_in;
} descriptor_bundle_t;

_Static_assert(sizeof(tusb_desc_interface_t) == 9u, "interface descriptor stub must be 9 bytes");
_Static_assert(sizeof(tusb_desc_endpoint_t) == 7u, "endpoint descriptor stub must be 7 bytes");
_Static_assert(sizeof(descriptor_bundle_t) == 30u, "Bluetooth descriptor bundle must be 30 bytes");

static const descriptor_bundle_t DESCRIPTORS = {
    .interface =
        {
            .bLength = sizeof(tusb_desc_interface_t),
            .bDescriptorType = TUSB_DESC_INTERFACE,
            .bInterfaceNumber = 0u,
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
};

static uint8_t *s_acl_out_buffer;
static uint8_t *s_control_buffer;
static uint16_t s_control_length;
static uint8_t s_last_xfer_ep;
static uint8_t *s_last_xfer_buffer;
static uint16_t s_last_xfer_length;
static unsigned s_xfer_calls;
static unsigned s_zlp_calls;
static unsigned s_claim_calls;
static unsigned s_release_calls;
static bool s_claim_result;
static bool s_xfer_result;

static uint8_t s_command_received[258];
static size_t s_command_received_len;
static unsigned s_command_received_count;
static uint8_t s_acl_received[RADIO_USB_BTH_ACL_MAX_SIZE];
static uint16_t s_acl_received_len;
static unsigned s_acl_received_count;
static uint16_t s_event_sent_len;
static unsigned s_event_sent_count;
static uint16_t s_acl_sent_len;
static unsigned s_acl_sent_count;
static unsigned s_protocol_error_count;

static void fake_reset(void) {
    s_acl_out_buffer = NULL;
    s_control_buffer = NULL;
    s_control_length = 0u;
    s_last_xfer_ep = 0u;
    s_last_xfer_buffer = NULL;
    s_last_xfer_length = 0u;
    s_xfer_calls = 0u;
    s_zlp_calls = 0u;
    s_claim_calls = 0u;
    s_release_calls = 0u;
    s_claim_result = true;
    s_xfer_result = true;

    memset(s_command_received, 0, sizeof(s_command_received));
    s_command_received_len = 0u;
    s_command_received_count = 0u;
    memset(s_acl_received, 0, sizeof(s_acl_received));
    s_acl_received_len = 0u;
    s_acl_received_count = 0u;
    s_event_sent_len = 0u;
    s_event_sent_count = 0u;
    s_acl_sent_len = 0u;
    s_acl_sent_count = 0u;
    s_protocol_error_count = 0u;
}

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
    s_last_xfer_ep = ep_addr;
    s_last_xfer_buffer = buffer;
    s_last_xfer_length = total_bytes;
    s_xfer_calls++;
    if (ep_addr == ACL_OUT_EP && total_bytes == FS_PACKET_SIZE) {
        s_acl_out_buffer = buffer;
    }
    if (ep_addr == ACL_IN_EP && buffer == NULL && total_bytes == 0u) {
        s_zlp_calls++;
    }
    return s_xfer_result;
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
    s_claim_calls++;
    return s_claim_result;
}

void usbd_edpt_release(uint8_t rhport, uint8_t ep_addr) {
    (void)rhport;
    (void)ep_addr;
    s_release_calls++;
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
    if (first->bDescriptorType != TUSB_DESC_ENDPOINT || second->bDescriptorType != TUSB_DESC_ENDPOINT ||
        first->bmAttributes.xfer != TUSB_XFER_BULK || second->bmAttributes.xfer != TUSB_XFER_BULK) {
        return false;
    }

    const tusb_desc_endpoint_t *out_ep = tu_edpt_dir(first->bEndpointAddress) == TUSB_DIR_OUT ? first : second;
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
    assert(command_len <= sizeof(s_command_received));
    memcpy(s_command_received, command, command_len);
    s_command_received_len = command_len;
    s_command_received_count++;
}

void radio_usb_bth_acl_received_cb(const uint8_t *acl, uint16_t acl_len) {
    assert(acl != NULL);
    assert(acl_len <= sizeof(s_acl_received));
    memcpy(s_acl_received, acl, acl_len);
    s_acl_received_len = acl_len;
    s_acl_received_count++;
}

void radio_usb_bth_event_sent_cb(uint16_t event_len) {
    s_event_sent_len = event_len;
    s_event_sent_count++;
}

void radio_usb_bth_acl_sent_cb(uint16_t acl_len) {
    s_acl_sent_len = acl_len;
    s_acl_sent_count++;
}

void radio_usb_bth_protocol_error_cb(void) { s_protocol_error_count++; }

static const usbd_class_driver_t *open_driver(void) {
    uint8_t count = 0u;
    const usbd_class_driver_t *driver = usbd_app_driver_get_cb(&count);
    assert(driver != NULL);
    assert(count == 1u);
    assert(strcmp(driver->name, "RADIO_BTH") == 0);
    assert(driver->init != NULL);
    assert(driver->open != NULL);
    assert(driver->control_xfer_cb != NULL);
    assert(driver->xfer_cb != NULL);

    driver->init();
    assert(driver->open(0u, &DESCRIPTORS.interface, sizeof(DESCRIPTORS)) == sizeof(DESCRIPTORS));
    assert(s_acl_out_buffer != NULL);
    assert(radio_usb_bth_event_ready());
    assert(radio_usb_bth_acl_ready());
    return driver;
}

static void test_driver_registration_and_open_validation(void) {
    fake_reset();
    uint8_t count = 0u;
    const usbd_class_driver_t *driver = usbd_app_driver_get_cb(&count);
    assert(driver != NULL && count == 1u);

    driver->init();
    descriptor_bundle_t bad = DESCRIPTORS;
    bad.interface.bInterfaceClass = 0xffu;
    assert(driver->open(0u, &bad.interface, sizeof(bad)) == 0u);

    driver->init();
    bad = DESCRIPTORS;
    bad.interface.bNumEndpoints = 2u;
    assert(driver->open(0u, &bad.interface, sizeof(bad)) == 0u);

    driver->init();
    assert(driver->open(0u, &DESCRIPTORS.interface, sizeof(DESCRIPTORS) - 1u) == 0u);
}

static tusb_control_request_t valid_command_request(uint16_t length) {
    tusb_control_request_t request = {0};
    request.bmRequestType_bit.type = TUSB_REQ_TYPE_CLASS;
    request.bmRequestType_bit.recipient = TUSB_REQ_RCPT_DEVICE;
    request.bRequest = 0u;
    request.wValue = 0u;
    request.wIndex = 0u;
    request.wLength = length;
    return request;
}

static void test_hci_command_control_transfer(void) {
    fake_reset();
    const usbd_class_driver_t *driver = open_driver();
    tusb_control_request_t request = valid_command_request(3u);

    assert(driver->control_xfer_cb(0u, CONTROL_STAGE_SETUP, &request));
    assert(s_control_buffer != NULL);
    assert(s_control_length == 3u);
    const uint8_t reset_command[] = {0x03u, 0x0cu, 0x00u};
    memcpy(s_control_buffer, reset_command, sizeof(reset_command));
    assert(driver->control_xfer_cb(0u, CONTROL_STAGE_DATA, &request));
    assert(s_command_received_count == 1u);
    assert(s_command_received_len == sizeof(reset_command));
    assert(memcmp(s_command_received, reset_command, sizeof(reset_command)) == 0);

    request.bmRequestType_bit.recipient = TUSB_REQ_RCPT_INTERFACE;
    request.wIndex = DESCRIPTORS.interface.bInterfaceNumber;
    assert(driver->control_xfer_cb(0u, CONTROL_STAGE_SETUP, &request));

    request.bRequest = 1u;
    assert(!driver->control_xfer_cb(0u, CONTROL_STAGE_SETUP, &request));

    request = valid_command_request(259u);
    assert(!driver->control_xfer_cb(0u, CONTROL_STAGE_SETUP, &request));
}

static void make_acl_packet(uint8_t *packet, uint16_t total_len) {
    assert(total_len >= 4u);
    const uint16_t payload_len = (uint16_t)(total_len - 4u);
    packet[0] = 0x01u;
    packet[1] = 0x20u;
    packet[2] = (uint8_t)(payload_len & 0xffu);
    packet[3] = (uint8_t)(payload_len >> 8);
    for (uint16_t i = 4u; i < total_len; ++i) {
        packet[i] = (uint8_t)(i * 13u + 7u);
    }
}

static void test_acl_out_reassembly(void) {
    fake_reset();
    const usbd_class_driver_t *driver = open_driver();
    uint8_t packet[68];
    make_acl_packet(packet, sizeof(packet));

    memcpy(s_acl_out_buffer, packet, FS_PACKET_SIZE);
    assert(driver->xfer_cb(0u, ACL_OUT_EP, XFER_RESULT_SUCCESS, FS_PACKET_SIZE));
    assert(s_acl_received_count == 0u);
    assert(s_protocol_error_count == 0u);

    memcpy(s_acl_out_buffer, packet + FS_PACKET_SIZE, sizeof(packet) - FS_PACKET_SIZE);
    assert(driver->xfer_cb(0u, ACL_OUT_EP, XFER_RESULT_SUCCESS,
                           sizeof(packet) - FS_PACKET_SIZE));
    assert(s_acl_received_count == 1u);
    assert(s_acl_received_len == sizeof(packet));
    assert(memcmp(s_acl_received, packet, sizeof(packet)) == 0);
    assert(s_protocol_error_count == 0u);
}

static void test_acl_out_fails_closed_on_incomplete_and_oversized_frames(void) {
    fake_reset();
    const usbd_class_driver_t *driver = open_driver();

    uint8_t incomplete[8] = {0x01u, 0x20u, 0x0au, 0x00u, 1u, 2u, 3u, 4u};
    memcpy(s_acl_out_buffer, incomplete, sizeof(incomplete));
    assert(driver->xfer_cb(0u, ACL_OUT_EP, XFER_RESULT_SUCCESS, sizeof(incomplete)));
    assert(s_acl_received_count == 0u);
    assert(s_protocol_error_count == 1u);

    const uint16_t oversized_payload = RADIO_USB_BTH_ACL_MAX_SIZE - 3u;
    uint8_t oversized_header[4] = {0x01u, 0x20u, (uint8_t)(oversized_payload & 0xffu),
                                   (uint8_t)(oversized_payload >> 8)};
    memcpy(s_acl_out_buffer, oversized_header, sizeof(oversized_header));
    assert(driver->xfer_cb(0u, ACL_OUT_EP, XFER_RESULT_SUCCESS, sizeof(oversized_header)));
    assert(s_acl_received_count == 0u);
    assert(s_protocol_error_count == 2u);
}

static void test_acl_out_zero_length_rules(void) {
    fake_reset();
    const usbd_class_driver_t *driver = open_driver();

    assert(driver->xfer_cb(0u, ACL_OUT_EP, XFER_RESULT_SUCCESS, 0u));
    assert(s_protocol_error_count == 0u);

    uint8_t packet[68];
    make_acl_packet(packet, sizeof(packet));
    memcpy(s_acl_out_buffer, packet, FS_PACKET_SIZE);
    assert(driver->xfer_cb(0u, ACL_OUT_EP, XFER_RESULT_SUCCESS, FS_PACKET_SIZE));
    assert(driver->xfer_cb(0u, ACL_OUT_EP, XFER_RESULT_SUCCESS, 0u));
    assert(s_protocol_error_count == 1u);
    assert(s_acl_received_count == 0u);
}

static void test_event_send_validation_and_completion(void) {
    fake_reset();
    const usbd_class_driver_t *driver = open_driver();
    const uint8_t event[] = {0x0eu, 0x01u, 0x00u};
    const uint8_t invalid_event[] = {0x0eu, 0x02u, 0x00u};

    assert(radio_usb_bth_event_send(event, sizeof(event)));
    assert(s_last_xfer_ep == EVENT_EP);
    assert(s_last_xfer_length == sizeof(event));
    assert(s_last_xfer_buffer != NULL);
    assert(memcmp(s_last_xfer_buffer, event, sizeof(event)) == 0);
    assert(driver->xfer_cb(0u, EVENT_EP, XFER_RESULT_SUCCESS, sizeof(event)));
    assert(s_event_sent_count == 1u);
    assert(s_event_sent_len == sizeof(event));

    assert(!radio_usb_bth_event_send(invalid_event, sizeof(invalid_event)));
    s_claim_result = false;
    assert(!radio_usb_bth_event_send(event, sizeof(event)));
}

static void test_acl_in_zlp_completion(void) {
    fake_reset();
    const usbd_class_driver_t *driver = open_driver();
    uint8_t packet[FS_PACKET_SIZE];
    make_acl_packet(packet, sizeof(packet));

    assert(radio_usb_bth_acl_send(packet, sizeof(packet)));
    assert(s_last_xfer_ep == ACL_IN_EP);
    assert(s_last_xfer_length == sizeof(packet));
    assert(s_last_xfer_buffer != NULL);
    assert(memcmp(s_last_xfer_buffer, packet, sizeof(packet)) == 0);

    assert(driver->xfer_cb(0u, ACL_IN_EP, XFER_RESULT_SUCCESS, sizeof(packet)));
    assert(s_zlp_calls == 1u);
    assert(s_acl_sent_count == 0u);
    assert(!radio_usb_bth_acl_ready());

    assert(driver->xfer_cb(0u, ACL_IN_EP, XFER_RESULT_SUCCESS, 0u));
    assert(s_acl_sent_count == 1u);
    assert(s_acl_sent_len == sizeof(packet));
    assert(radio_usb_bth_acl_ready());
}

static void test_acl_in_validation_and_transfer_error(void) {
    fake_reset();
    const usbd_class_driver_t *driver = open_driver();
    uint8_t invalid_acl[5] = {0x01u, 0x20u, 0x02u, 0x00u, 0xaau};
    assert(!radio_usb_bth_acl_send(invalid_acl, sizeof(invalid_acl)));

    uint8_t packet[12];
    make_acl_packet(packet, sizeof(packet));
    assert(radio_usb_bth_acl_send(packet, sizeof(packet)));
    assert(driver->xfer_cb(0u, ACL_IN_EP, XFER_RESULT_FAILED, sizeof(packet)));
    assert(s_protocol_error_count == 1u);
}

static void test_send_failure_releases_claim(void) {
    fake_reset();
    (void)open_driver();
    const uint8_t event[] = {0x0eu, 0x01u, 0x00u};

    s_xfer_result = false;
    const unsigned releases_before = s_release_calls;
    assert(!radio_usb_bth_event_send(event, sizeof(event)));
    assert(s_release_calls == releases_before + 1u);
}

static void test_reset_closes_interface(void) {
    fake_reset();
    const usbd_class_driver_t *driver = open_driver();
    driver->reset(0u);
    assert(!radio_usb_bth_event_ready());
    assert(!radio_usb_bth_acl_ready());
}

int main(void) {
    test_driver_registration_and_open_validation();
    test_hci_command_control_transfer();
    test_acl_out_reassembly();
    test_acl_out_fails_closed_on_incomplete_and_oversized_frames();
    test_acl_out_zero_length_rules();
    test_event_send_validation_and_completion();
    test_acl_in_zlp_completion();
    test_acl_in_validation_and_transfer_error();
    test_send_failure_releases_claim();
    test_reset_closes_interface();
    puts("radio_usb_bth host tests passed");
    return 0;
}
