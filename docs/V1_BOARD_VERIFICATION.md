# V1 Development Board Verification

This document records the board-level evidence for V1-102 of `ESP32_RADIO_DONGLE_V1_TODO.md`.

V1-102 is complete: the two development boards selected for V1 bring-up were already identified by the project owner through the exact purchase listings, prior board photographs, and prior USB-enumeration evidence. The reference GPIO/native-USB requirements have been checked against those boards.

## Reference interconnect

| Signal | ESP32-S3 side | ESP32-WROOM-32 side |
| --- | --- | --- |
| HCI TX | GPIO4 | GPIO16 RX |
| HCI RX | GPIO5 | GPIO17 TX |
| RTS | GPIO6 | GPIO25 CTS |
| CTS | GPIO7 | GPIO26 RTS |
| Ground | GND | GND |

TX/RX and RTS/CTS cross between the two endpoints.

## Selected ESP32-S3 board

### Board identity

The V1 ESP32-S3 board is the AYWHP ESP32-S3-DevKitC-1-N16R8 development board purchased from:

- https://www.amazon.com/dp/B0DG8L5NG5
- ASIN: `B0DG8L5NG5`
- Product family: ESP32-S3-DevKitC-1 / ESP32-S3-WROOM-1
- Flash/PSRAM variant: N16R8
- Development-board USB connectors: dual USB Type-C

Prior Linux enumeration supplied by the project owner for this physical S3 board was:

```text
Bus 001 Device 008: ID 303a:4001 ESP32 Macro Keyboard Project ESP32 Macro Keyboard
```

That prior custom USB-device enumeration is direct project evidence that the board exposes a working ESP32-S3 native USB device path rather than only a USB-UART bridge.

### GPIO/native-USB compatibility

Espressif's ESP32-S3-DevKitC-1 documentation establishes that:

- GPIO4, GPIO5, GPIO6, and GPIO7 are broken out consecutively on header J1 as general-purpose I/O.
- GPIO19 and GPIO20 are the native USB D- and D+ signals.
- The board design provides a separate ESP32-S3 full-speed USB OTG/device connection.
- GPIO4-7 therefore do not collide with the native USB D-/D+ signals used by V1.

Official reference:

- https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/

### V1 result

The selected ESP32-S3 board satisfies the V1 pin and native-USB requirements:

- GPIO4: available for HCI TX.
- GPIO5: available for HCI RX.
- GPIO6: available for HCI RTS.
- GPIO7: available for HCI CTS.
- Native USB device path: available and previously exercised by project firmware.

## Selected ESP32-WROOM-32 board

### Board identity

The V1 original-ESP32 board is the Aideepen 30-pin ESP-WROOM-32 development board purchased from:

- https://www.amazon.com/dp/B0BQJ8BTVB
- ASIN: `B0BQJ8BTVB`
- Product family: ESP-WROOM-32 / ESP32S 30-pin development board
- USB interface: Micro-USB through onboard CP2102 USB-to-UART bridge

Prior Linux enumeration supplied by the project owner for this physical WROOM board was:

```text
Bus 001 Device 010: ID 10c4:ea60 Silicon Labs CP210x UART Bridge
```

The CP210x identity is consistent with the selected listing's CP2102 USB-UART bridge. This USB connector remains a flashing/debugging path only; it is not the V1 host-facing Bluetooth USB connection.

### GPIO compatibility

The selected board uses the standard 30-pin ESP-WROOM-32 development-board layout represented in the product listing and prior project photographs. The required signals are physically broken out:

- GPIO16 / RX2
- GPIO17 / TX2
- GPIO25
- GPIO26

The underlying ESP-WROOM-32 module exposes GPIO16 and GPIO17 as UART2 RX/TX-capable GPIOs and GPIO25/GPIO26 as normal bidirectional GPIOs. This is a WROOM board, not a WROVER variant, so the WROVER-specific GPIO16/17 reservation does not apply.

Official module/reference-board references:

- https://documentation.espressif.com/esp32-wroom-32_datasheet_en.html
- https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32/esp32-devkitc/user_guide.html

### V1 result

The selected ESP32-WROOM-32 board satisfies the V1 inter-MCU pin requirements:

- GPIO16: available for HCI RX.
- GPIO17: available for HCI TX.
- GPIO25: available for HCI CTS.
- GPIO26: available for HCI RTS.
- UART0/CP2102 path remains separate for flashing and development logging.

## Reference-board conflict notes

These notes remain useful if the project later substitutes hardware.

### ESP32-S3-USB-OTG

Espressif's ESP32-S3-USB-OTG board is **not** a drop-in match for the current GPIO4-7 assignment because it uses those pins for its onboard LCD:

- GPIO4: LCD data/command
- GPIO5: LCD enable
- GPIO6: LCD SPI clock
- GPIO7: LCD SPI MOSI

Reference:

- https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-usb-otg/user_guide.html

### ESP32-WROVER variants

WROVER-equipped original-ESP32 boards are **not** a drop-in match for the current GPIO16/17 assignment because those pins are reserved for internal use on WROVER variants. The selected V1 board is ESP-WROOM-32, so this restriction does not apply.

## V1-102 disposition

**V1-102: PASS.**

Evidence used:

1. Exact project-owner purchase listing for the S3 board: ASIN `B0DG8L5NG5`.
2. Exact project-owner purchase listing for the WROOM board: ASIN `B0BQJ8BTVB`.
3. Prior physical-board photographs supplied by the project owner.
4. Prior S3 Linux USB enumeration as `303a:4001` running a project USB device firmware.
5. Prior WROOM Linux enumeration as Silicon Labs CP210x `10c4:ea60`.
6. Espressif ESP32-S3-DevKitC-1 GPIO/native-USB documentation.
7. Espressif ESP-WROOM-32/DevKitC GPIO documentation.

No additional physical test is required merely to identify or approve the selected boards. Device testing is still required for V1-103/V1-104 and later electrical, HCI, USB-enumeration, and operating-system acceptance gates.
