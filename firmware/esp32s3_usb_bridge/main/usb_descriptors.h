#ifndef USB_DESCRIPTORS_H
#define USB_DESCRIPTORS_H

#include <stdint.h>

#include "tusb.h"

#ifdef __cplusplus
extern "C" {
#endif

void radio_usb_descriptors_init(void);
const tusb_desc_device_t *radio_usb_device_descriptor(void);
const uint8_t *radio_usb_full_speed_configuration_descriptor(void);
const char **radio_usb_string_descriptors(void);
int radio_usb_string_descriptor_count(void);

#ifdef __cplusplus
}
#endif

#endif
