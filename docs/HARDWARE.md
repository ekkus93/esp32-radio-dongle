# ESP32 Radio Dongle V1 Hardware

## Selected prototype boards

V1 uses these verified development boards for initial bring-up:

- **ESP32-S3:** AYWHP ESP32-S3-DevKitC-1-N16R8, ASIN `B0DG8L5NG5`.
- **ESP32-WROOM-32:** Aideepen 30-pin ESP-WROOM-32 development board, ASIN `B0BQJ8BTVB`.

Board-selection evidence and substitution caveats are recorded in `docs/V1_BOARD_VERIFICATION.md`.

## Prototype topology

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

This is the V1 reference pin assignment. Change it only through an intentional documented hardware revision.

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

## Verified pin/native-USB compatibility

V1-102 is complete.

For the selected S3 board, GPIO4-7 are available for the HCI link and the board exposes the ESP32-S3 native USB device path. Prior project firmware has already exercised that native USB path.

For the selected WROOM board, GPIO16, GPIO17, GPIO25, and GPIO26 are physically available. The board's CP210x USB-UART connection remains a development flashing/logging path only.

This compatibility evidence approves the board/pin contract; it does **not** substitute for the deferred electrical/UART tests.

## UART routing implementation note

The WROOM firmware owns UART2 rather than depending on fixed HCI-UART pin defaults. It explicitly routes UART2 as:

```text
TX  = GPIO17
RX  = GPIO16
RTS = GPIO26
CTS = GPIO25
```

ESP32 GPIO Matrix routing makes the chosen CTS/RTS assignment valid even though GPIO25/26 are not inherently fixed UART2 flow-control pins.

The WROOM Bluetooth controller is exposed to application bridge code through VHCI, while the application provides the external H4 UART transport. This lets the project enforce the reference pinout, bounded framing, queues, diagnostics, and recovery semantics.

## Prototype power

For the two-development-board prototype:

1. Power the ESP32-S3 development board from its own USB connection.
2. Power the ESP32-WROOM-32 development board independently from its own USB connection during development.
3. Connect the two board grounds.
4. **Do not connect the two boards' regulated 3.3 V output rails together.**
5. Connect only the HCI TX/RX/RTS/CTS signals and common ground shown above unless a later documented revision says otherwise.

Both selected MCU boards use 3.3 V GPIO logic, so the reference UART connection does not require a logic-level translator.

## Development USB usage

During bring-up it is normal to have two host cables:

```text
PC USB #1 -> ESP32-S3 native USB       (eventual product-facing USB)
PC USB #2 -> WROOM CP210x USB-UART     (development flash/log only)
```

The WROOM USB-UART path is not part of the final Bluetooth data path. UART0 remains available for development flashing/logging where practical.

## Hardware validation still required

Board identity and pin/native-USB compatibility are already verified. V1-G10 remains open only for the physical behavior that has intentionally been deferred:

- connect and verify common ground;
- verify S3 -> WROOM UART traffic;
- verify WROOM -> S3 UART traffic;
- verify both RTS/CTS crossings assert, throttle, and release correctly;
- verify traffic resumes without corruption after backpressure;
- verify reset/boot behavior with the wiring attached; and
- confirm no physical board-specific reset/bootstrap issue appears in the assembled prototype.

The dedicated smoke firmware and bench procedure for these tests are documented in `docs/V1_UART_BRINGUP.md`.
