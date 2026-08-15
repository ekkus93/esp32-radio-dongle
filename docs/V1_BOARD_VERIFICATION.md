# V1 Development Board Verification

This document records the board-level evidence for V1-102 of `ESP32_RADIO_DONGLE_V1_TODO.md`.

V1-102 is complete only after the exact two development boards used for bring-up are identified and checked against the reference pinout. The compatibility results below establish known-good reference boards and known conflicts; they do not substitute for identifying the physical boards in hand.

## Reference interconnect

| Signal | ESP32-S3 side | ESP32-WROOM-32 side |
| --- | --- | --- |
| HCI TX | GPIO4 | GPIO16 RX |
| HCI RX | GPIO5 | GPIO17 TX |
| RTS | GPIO6 | GPIO25 CTS |
| CTS | GPIO7 | GPIO26 RTS |
| Ground | GND | GND |

TX/RX and RTS/CTS cross between the two endpoints.

## ESP32-S3 reference-board compatibility

### ESP32-S3-DevKitC-1: compatible with the current pin assignment

Espressif's ESP32-S3-DevKitC-1 documentation shows:

- GPIO4, GPIO5, GPIO6, and GPIO7 are broken out on header J1 as general-purpose I/O.
- The board provides a separate `ESP32-S3 USB Port`, which is the ESP32-S3 full-speed USB OTG interface.
- GPIO19 and GPIO20 are the S3 USB D- and D+ signals; the dedicated USB connector handles them, so the HCI GPIO4-7 assignment does not collide with USB.
- Board variants may differ in flash/PSRAM and RGB-LED wiring, but the documented GPIO4-7 header positions remain available on the DevKitC-1 family.

Official reference:

- https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/

For V1, the host-facing cable must use the native ESP32-S3 USB/OTG connector, not merely the USB-to-UART bridge connector.

### ESP32-S3-USB-OTG: conflicts with the current GPIO4-7 assignment

Espressif's ESP32-S3-USB-OTG board uses the four proposed HCI pins for its onboard LCD:

- GPIO4: LCD data/command
- GPIO5: LCD enable
- GPIO6: LCD SPI clock
- GPIO7: LCD SPI MOSI

Therefore this board is not a clean match for the current V1 reference pinout unless those onboard functions are intentionally disabled/reworked or the inter-MCU pin assignment is revised.

Official reference:

- https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-usb-otg/user_guide.html

## ESP32-WROOM-32 reference-board compatibility

### ESP32-DevKitC V4 with an ESP32-WROOM module: compatible

Espressif's ESP32-DevKitC V4 documentation shows:

- GPIO25 and GPIO26 are exposed on header J2.
- GPIO17 and GPIO16 are exposed on header J3 when the board is populated with an ESP32-WROOM or ESP32-SOLO-1 module.
- The board's USB connector is a USB-to-UART path; it is for WROOM flashing/logging in this project, not the final Bluetooth host connection.

Official reference:

- https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32/esp32-devkitc/user_guide.html

### ESP32-DevKitC V4 with an ESP32-WROVER module: incompatible with the current pin assignment

Espressif explicitly documents GPIO16 and GPIO17 as reserved for internal use on WROVER-equipped DevKitC variants. A WROVER board therefore does not satisfy the current V1 WROOM-side pin contract without changing pins.

## Evidence required from the physical boards

Record all of the following before checking V1-102 complete:

### ESP32-S3 board

1. Board product/model printed on the PCB, if present.
2. Module marking printed on the RF module can, for example `ESP32-S3-WROOM-1-N8R8`.
3. Board revision, if printed.
4. Confirmation that GPIO4, GPIO5, GPIO6, and GPIO7 are physically exposed and not dedicated to unavoidable onboard peripherals.
5. Identification of the connector wired to the S3 native USB D+/D- interface.

### ESP32-WROOM-32 board

1. Board product/model printed on the PCB, if present.
2. Module marking printed on the RF module can; it must be a WROOM-family part for the current GPIO16/17 assumption.
3. Board revision, if printed.
4. Confirmation that GPIO16, GPIO17, GPIO25, and GPIO26 are physically exposed.

For an unbranded/generic clone, photographs of the front and back plus a readable module-can marking are sufficient evidence to identify the relevant electrical layout. Do not infer compatibility from the ESP32 chip name alone.

## V1-102 current status

- Reference ESP32-S3-DevKitC-1 compatibility: **verified from Espressif documentation**.
- Reference ESP32-DevKitC V4 + WROOM compatibility: **verified from Espressif documentation**.
- ESP32-S3-USB-OTG GPIO4-7 conflict: **verified from Espressif documentation**.
- WROVER GPIO16/17 conflict: **verified from Espressif documentation**.
- Exact ESP32-S3 development board used for bring-up: **not yet identified**.
- Exact ESP32-WROOM-32 development board used for bring-up: **not yet identified**.

V1-102 remains open until the last two items are resolved and the corresponding pin/access checks are recorded.
