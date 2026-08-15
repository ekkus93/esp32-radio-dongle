#ifndef TEST_STUB_DEVICE_USBD_PVT_H
#define TEST_STUB_DEVICE_USBD_PVT_H

#include <stdbool.h>
#include <stdint.h>

#include "tusb.h"

typedef struct {
    const char *name;
    void (*init)(void);
    bool (*deinit)(void);
    void (*reset)(uint8_t rhport);
    uint16_t (*open)(uint8_t rhport, const tusb_desc_interface_t *interface, uint16_t max_len);
    bool (*control_xfer_cb)(uint8_t rhport, uint8_t stage, const tusb_control_request_t *request);
    bool (*xfer_cb)(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes);
    void *xfer_isr;
    void *sof;
} usbd_class_driver_t;

#endif
