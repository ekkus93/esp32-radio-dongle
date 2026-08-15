#include "radio_usb_bth.h"

#include <string.h>

#include "device/usbd.h"
#include "device/usbd_pvt.h"
#include "tusb.h"

#define RADIO_USB_BTH_HCI_COMMAND_MAX_SIZE 258u
#define RADIO_USB_BTH_FS_PACKET_SIZE 64u

#define RADIO_USB_BTH_APP_SUBCLASS 0x01u
#define RADIO_USB_BTH_PRIMARY_CONTROLLER_PROTOCOL 0x01u

#define RADIO_USB_BTH_PRIMARY_DESC_LEN                                                             \
    (sizeof(tusb_desc_interface_t) + (3u * sizeof(tusb_desc_endpoint_t)))

typedef struct {
    uint8_t interface_number;
    uint8_t event_in_ep;
    uint8_t acl_in_ep;
    uint8_t acl_out_ep;
    uint16_t acl_in_packet_size;
    bool opened;
    bool acl_zlp_pending;
    uint16_t acl_zlp_payload_len;
    size_t acl_rx_used;
    size_t acl_rx_expected;
} radio_usb_bth_state_t;

typedef struct {
    TUD_EPBUF_DEF(acl_out_ep, RADIO_USB_BTH_FS_PACKET_SIZE);
    TUD_EPBUF_DEF(hci_command, RADIO_USB_BTH_HCI_COMMAND_MAX_SIZE);
    TUD_EPBUF_DEF(event_in, RADIO_USB_BTH_EVENT_MAX_SIZE);
    TUD_EPBUF_DEF(acl_in, RADIO_USB_BTH_ACL_MAX_SIZE);
    uint8_t acl_rx[RADIO_USB_BTH_ACL_MAX_SIZE];
} radio_usb_bth_buffers_t;

static radio_usb_bth_state_t s_state;
CFG_TUD_MEM_SECTION static radio_usb_bth_buffers_t s_buffers;

TU_ATTR_WEAK void radio_usb_bth_hci_command_received_cb(const uint8_t *command,
                                                        size_t command_len) {
    (void)command;
    (void)command_len;
}

TU_ATTR_WEAK void radio_usb_bth_acl_received_cb(const uint8_t *acl, uint16_t acl_len) {
    (void)acl;
    (void)acl_len;
}

TU_ATTR_WEAK void radio_usb_bth_event_sent_cb(uint16_t event_len) { (void)event_len; }

TU_ATTR_WEAK void radio_usb_bth_acl_sent_cb(uint16_t acl_len) { (void)acl_len; }

TU_ATTR_WEAK void radio_usb_bth_protocol_error_cb(void) {}

static uint16_t read_le16(const uint8_t *bytes) {
    return (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8);
}

static bool event_packet_valid(const uint8_t *event, uint16_t event_len) {
    if (event == NULL || event_len < 2u || event_len > RADIO_USB_BTH_EVENT_MAX_SIZE) {
        return false;
    }
    return event_len == (uint16_t)(2u + event[1]);
}

static bool acl_packet_valid(const uint8_t *acl, uint16_t acl_len) {
    if (acl == NULL || acl_len < 4u || acl_len > RADIO_USB_BTH_ACL_MAX_SIZE) {
        return false;
    }
    return acl_len == (uint16_t)(4u + read_le16(&acl[2]));
}

static void acl_rx_reset(void) {
    s_state.acl_rx_used = 0u;
    s_state.acl_rx_expected = 0u;
}

static bool arm_acl_out(uint8_t rhport) {
    return usbd_edpt_xfer(rhport, s_state.acl_out_ep, s_buffers.acl_out_ep,
                          sizeof(s_buffers.acl_out_ep));
}

static void protocol_error_and_reset_acl_rx(void) {
    acl_rx_reset();
    radio_usb_bth_protocol_error_cb();
}

static bool ingest_acl_out_chunk(const uint8_t *chunk, uint32_t chunk_len) {
    if (chunk_len == 0u) {
        if (s_state.acl_rx_used != 0u) {
            protocol_error_and_reset_acl_rx();
            return false;
        }
        return true;
    }

    if (chunk_len > RADIO_USB_BTH_FS_PACKET_SIZE ||
        s_state.acl_rx_used + chunk_len > sizeof(s_buffers.acl_rx)) {
        protocol_error_and_reset_acl_rx();
        return false;
    }

    memcpy(&s_buffers.acl_rx[s_state.acl_rx_used], chunk, chunk_len);
    s_state.acl_rx_used += chunk_len;

    if (s_state.acl_rx_expected == 0u && s_state.acl_rx_used >= 4u) {
        const size_t payload_len = read_le16(&s_buffers.acl_rx[2]);
        s_state.acl_rx_expected = 4u + payload_len;
        if (s_state.acl_rx_expected > sizeof(s_buffers.acl_rx)) {
            protocol_error_and_reset_acl_rx();
            return false;
        }
    }

    if (s_state.acl_rx_expected != 0u) {
        if (s_state.acl_rx_used > s_state.acl_rx_expected) {
            protocol_error_and_reset_acl_rx();
            return false;
        }
        if (s_state.acl_rx_used == s_state.acl_rx_expected) {
            const uint16_t packet_len = (uint16_t)s_state.acl_rx_used;
            radio_usb_bth_acl_received_cb(s_buffers.acl_rx, packet_len);
            acl_rx_reset();
            return true;
        }
    }

    /* A short bulk packet ends the host transfer. If the HCI length says the
     * packet is incomplete, fail closed instead of carrying an ambiguous frame
     * into the next transfer. */
    if (chunk_len < RADIO_USB_BTH_FS_PACKET_SIZE) {
        protocol_error_and_reset_acl_rx();
        return false;
    }

    return true;
}

bool radio_usb_bth_event_ready(void) {
    return s_state.opened && !usbd_edpt_busy(0, s_state.event_in_ep) &&
           !usbd_edpt_stalled(0, s_state.event_in_ep);
}

bool radio_usb_bth_acl_ready(void) {
    return s_state.opened && !usbd_edpt_busy(0, s_state.acl_in_ep) &&
           !usbd_edpt_stalled(0, s_state.acl_in_ep) && !s_state.acl_zlp_pending;
}

bool radio_usb_bth_event_send(const uint8_t *event, uint16_t event_len) {
    if (!s_state.opened || !event_packet_valid(event, event_len) ||
        !usbd_edpt_claim(0, s_state.event_in_ep)) {
        return false;
    }

    memcpy(s_buffers.event_in, event, event_len);
    if (!usbd_edpt_xfer(0, s_state.event_in_ep, s_buffers.event_in, event_len)) {
        usbd_edpt_release(0, s_state.event_in_ep);
        return false;
    }
    return true;
}

bool radio_usb_bth_acl_send(const uint8_t *acl, uint16_t acl_len) {
    if (!s_state.opened || !acl_packet_valid(acl, acl_len) || s_state.acl_zlp_pending ||
        !usbd_edpt_claim(0, s_state.acl_in_ep)) {
        return false;
    }

    memcpy(s_buffers.acl_in, acl, acl_len);
    if (!usbd_edpt_xfer(0, s_state.acl_in_ep, s_buffers.acl_in, acl_len)) {
        usbd_edpt_release(0, s_state.acl_in_ep);
        return false;
    }
    return true;
}

static void class_init(void) { memset(&s_state, 0, sizeof(s_state)); }

static bool class_deinit(void) {
    memset(&s_state, 0, sizeof(s_state));
    return true;
}

static void class_reset(uint8_t rhport) {
    (void)rhport;
    memset(&s_state, 0, sizeof(s_state));
}

static uint16_t class_open(uint8_t rhport, const tusb_desc_interface_t *interface,
                           uint16_t max_len) {
    if (interface == NULL || interface->bInterfaceClass != TUSB_CLASS_WIRELESS_CONTROLLER ||
        interface->bInterfaceSubClass != RADIO_USB_BTH_APP_SUBCLASS ||
        interface->bInterfaceProtocol != RADIO_USB_BTH_PRIMARY_CONTROLLER_PROTOCOL ||
        interface->bNumEndpoints != 3u || max_len < RADIO_USB_BTH_PRIMARY_DESC_LEN) {
        return 0u;
    }

    const tusb_desc_endpoint_t *endpoint = (const tusb_desc_endpoint_t *)tu_desc_next(interface);
    if (endpoint->bDescriptorType != TUSB_DESC_ENDPOINT ||
        endpoint->bmAttributes.xfer != TUSB_XFER_INTERRUPT ||
        tu_edpt_dir(endpoint->bEndpointAddress) != TUSB_DIR_IN ||
        !usbd_edpt_open(rhport, endpoint)) {
        return 0u;
    }
    s_state.event_in_ep = endpoint->bEndpointAddress;

    endpoint = (const tusb_desc_endpoint_t *)tu_desc_next(endpoint);
    if (!usbd_open_edpt_pair(rhport, (const uint8_t *)endpoint, 2u, TUSB_XFER_BULK,
                             &s_state.acl_out_ep, &s_state.acl_in_ep)) {
        return 0u;
    }

    const tusb_desc_endpoint_t *acl_endpoint = endpoint;
    for (size_t i = 0u; i < 2u; ++i) {
        if (tu_edpt_dir(acl_endpoint->bEndpointAddress) == TUSB_DIR_IN) {
            s_state.acl_in_packet_size = tu_edpt_packet_size(acl_endpoint);
            break;
        }
        acl_endpoint = (const tusb_desc_endpoint_t *)tu_desc_next(acl_endpoint);
    }

    if (s_state.acl_in_packet_size == 0u || !arm_acl_out(rhport)) {
        return 0u;
    }

    s_state.interface_number = interface->bInterfaceNumber;
    s_state.opened = true;
    acl_rx_reset();
    return RADIO_USB_BTH_PRIMARY_DESC_LEN;
}

static bool command_request_matches(const tusb_control_request_t *request) {
    if (request->bmRequestType_bit.direction != TUSB_DIR_OUT ||
        request->bmRequestType_bit.type != TUSB_REQ_TYPE_CLASS) {
        return false;
    }

    /* Bluetooth USB HCI historically accepts any host-to-device class request
     * addressed to the Controller as an HCI command, even if bRequest, wValue,
     * or (for a device-targeted request) wIndex differ from the recommended
     * 0x00 values. Keep interface-targeted routing strict so this remains safe
     * if the device later becomes composite. */
    if (request->bmRequestType_bit.recipient == TUSB_REQ_RCPT_DEVICE) {
        return true;
    }
    if (request->bmRequestType_bit.recipient == TUSB_REQ_RCPT_INTERFACE) {
        return request->wIndex == s_state.interface_number;
    }
    return false;
}

static bool class_control_xfer(uint8_t rhport, uint8_t stage,
                               const tusb_control_request_t *request) {
    if (!s_state.opened || request == NULL || !command_request_matches(request) ||
        request->wLength > sizeof(s_buffers.hci_command)) {
        return false;
    }

    if (stage == CONTROL_STAGE_SETUP) {
        return tud_control_xfer(rhport, request, s_buffers.hci_command, request->wLength);
    }
    if (stage == CONTROL_STAGE_DATA) {
        radio_usb_bth_hci_command_received_cb(s_buffers.hci_command, request->wLength);
    }
    return true;
}

static bool class_xfer(uint8_t rhport, uint8_t ep_addr, xfer_result_t result,
                       uint32_t xferred_bytes) {
    if (!s_state.opened) {
        return false;
    }

    if (ep_addr == s_state.acl_out_ep) {
        if (result != XFER_RESULT_SUCCESS) {
            radio_usb_bth_protocol_error_cb();
        } else {
            (void)ingest_acl_out_chunk(s_buffers.acl_out_ep, xferred_bytes);
        }
        return arm_acl_out(rhport);
    }

    if (ep_addr == s_state.event_in_ep) {
        if (result != XFER_RESULT_SUCCESS) {
            radio_usb_bth_protocol_error_cb();
            return true;
        }
        radio_usb_bth_event_sent_cb((uint16_t)xferred_bytes);
        return true;
    }

    if (ep_addr == s_state.acl_in_ep) {
        if (result != XFER_RESULT_SUCCESS) {
            s_state.acl_zlp_pending = false;
            radio_usb_bth_protocol_error_cb();
            return true;
        }

        if (!s_state.acl_zlp_pending && xferred_bytes > 0u &&
            (xferred_bytes % s_state.acl_in_packet_size) == 0u) {
            s_state.acl_zlp_pending = true;
            s_state.acl_zlp_payload_len = (uint16_t)xferred_bytes;
            if (!usbd_edpt_xfer(rhport, s_state.acl_in_ep, NULL, 0u)) {
                s_state.acl_zlp_pending = false;
                radio_usb_bth_protocol_error_cb();
            }
            return true;
        }

        if (s_state.acl_zlp_pending && xferred_bytes == 0u) {
            const uint16_t payload_len = s_state.acl_zlp_payload_len;
            s_state.acl_zlp_pending = false;
            s_state.acl_zlp_payload_len = 0u;
            radio_usb_bth_acl_sent_cb(payload_len);
            return true;
        }

        radio_usb_bth_acl_sent_cb((uint16_t)xferred_bytes);
        return true;
    }

    return false;
}

static const usbd_class_driver_t s_radio_bth_driver = {
    .name = "RADIO_BTH",
    .init = class_init,
    .deinit = class_deinit,
    .reset = class_reset,
    .open = class_open,
    .control_xfer_cb = class_control_xfer,
    .xfer_cb = class_xfer,
    .xfer_isr = NULL,
    .sof = NULL,
};

usbd_class_driver_t const *usbd_app_driver_get_cb(uint8_t *driver_count) {
    if (driver_count == NULL) {
        return NULL;
    }
    *driver_count = 1u;
    return &s_radio_bth_driver;
}
