# ESP32 Radio Dongle V1 Hardware

## Prototype topology

V1 uses two development boards during bring-up:

```text
Host PC
  |
  | native USB
  v
+-------------------+
| ESP32-S3          |
| USB/HCI bridge    |
+-------------------+
   | TX  RX  RTS CTS
   |  \ /    \ /
   |  / \    / \
+-------------------+
| ESP32-WROOM-32    |
| BT controller     |
+-------------------+
        ))) Bluetooth Classic + BLE
```

The ESP32-S3 connector used by the host must reach the S3 native USB peripheral. A connector wired only to a USB-UART bridge is not the V1 host-facing connection.

## Reference interconnect

This is the provisional-final V1 pin assignment. Change it only through an intentional documented hardware revision.

| Signal | ESP32-S3 | Direction | ESP32-WROOM-32 |
|---|---:|:---:|---:|
| HCI TX | GPIO4 | -> | GPIO16 / UART2 RX |
| HCI RX | GPIO5 | <- | GPIO17 / UART2 TX |
| HCI RTS | GPIO6 | -> | GPIO25 / UART2 CTS |
| HCI CTS | GPIO7 | <- | GPIO26 / UART2 RTS |
| Ground | GND | <-> | GND |

The signal pairs are deliberately crossed:

```text
S3 GPIO4  TX  ----------------> WROOM GPIO16 RX
S3 GPIO5  RX  <---------------- WROOM GPIO17 TX
S3 GPIO6  RTS ----------------> WROOM GPIO25 CTS
S3 GPIO7  CTS <---------------- WROOM GPIO26 RTS
S3 GND        ----------------- WROOM GND
```

## UART routing implementation note

The WROOM firmware owns UART2 rather than depending on the ESP-IDF controller's fixed HCI-UART pin defaults. It SHALL explicitly call the ESP-IDF UART pin-routing API so UART2 uses:

```text
TX  = GPIO17
RX  = GPIO16
RTS = GPIO26
CTS = GPIO25
```

ESP-IDF routes UART signals through the GPIO Matrix when the selected GPIO does not match that UART signal's IO-MUX pin. This is intentional for the reference flow-control pins.

The WROOM Bluetooth controller is therefore exposed to the application through VHCI, while the application provides the external H4 UART transport. This keeps the externally visible protocol standard HCI H4 while allowing the project to enforce its own pinout, bounded framing, queues, diagnostics, and recovery semantics.

## Prototype power

For the initial two-development-board prototype:

1. Power the ESP32-S3 development board from its USB connection.
2. The ESP32-WROOM-32 development board may be powered independently from its own USB connection during flashing/debugging.
3. Connect the two board grounds.
4. **Do not connect the two development boards' regulated 3.3 V output rails together.**
5. Connect only the HCI UART signals and common ground shown above unless a later documented revision says otherwise.

Both MCU GPIO domains are 3.3 V, so the reference UART connection does not require a logic-level translator when using normal 3.3 V ESP32 development boards.

## Development USB usage

During bring-up it is normal to have two host cables:

```text
PC USB #1 -> ESP32-S3 native USB       (eventual product-facing USB)
PC USB #2 -> WROOM USB-UART connector  (development flash/log only)
```

The WROOM development USB-UART path is not part of the final Bluetooth data path. UART0 should remain available for development flashing/logging where practical.

## Hardware validation still required

Documentation of the reference pinout does not replace physical acceptance. V1-G10 remains open until the exact boards used for bring-up are identified and the following are measured/tested on hardware:

- GPIO4-7 availability on the selected S3 board;
- native USB availability on the selected S3 board;
- GPIO16/17/25/26 availability on the selected WROOM board;
- bidirectional UART traffic;
- RTS/CTS assertion and release;
- reset/boot behavior with the wiring attached; and
- absence of board-specific bootstrap/peripheral conflicts.
