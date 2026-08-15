#ifndef TEST_STUB_DEVICE_USBD_H
#define TEST_STUB_DEVICE_USBD_H

#include <stdbool.h>
#include <stdint.h>

#include "tusb.h"

bool usbd_edpt_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t *buffer, uint16_t total_bytes);
bool usbd_edpt_busy(uint8_t rhport, uint8_t ep_addr);
bool usbd_edpt_stalled(uint8_t rhport, uint8_t ep_addr);
bool usbd_edpt_claim(uint8_t rhport, uint8_t ep_addr);
void usbd_edpt_release(uint8_t rhport, uint8_t ep_addr);
bool usbd_edpt_open(uint8_t rhport, const tusb_desc_endpoint_t *endpoint);
bool usbd_open_edpt_pair(uint8_t rhport, const uint8_t *descriptor, uint8_t endpoint_count,
                         uint8_t transfer_type, uint8_t *ep_out, uint8_t *ep_in);

#endif
