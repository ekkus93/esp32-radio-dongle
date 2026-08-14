#ifndef RADIO_USB_BTH_H
#define RADIO_USB_BTH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RADIO_USB_BTH_EVENT_MAX_SIZE 257u
#define RADIO_USB_BTH_ACL_MAX_SIZE 2052u

bool radio_usb_bth_event_ready(void);
bool radio_usb_bth_acl_ready(void);
bool radio_usb_bth_event_send(const uint8_t *event, uint16_t event_len);
bool radio_usb_bth_acl_send(const uint8_t *acl, uint16_t acl_len);

/* Strong application implementations may override these weak callbacks. */
void radio_usb_bth_hci_command_received_cb(const uint8_t *command, size_t command_len);
void radio_usb_bth_acl_received_cb(const uint8_t *acl, uint16_t acl_len);
void radio_usb_bth_event_sent_cb(uint16_t event_len);
void radio_usb_bth_acl_sent_cb(uint16_t acl_len);
void radio_usb_bth_protocol_error_cb(void);

#ifdef __cplusplus
}
#endif

#endif
