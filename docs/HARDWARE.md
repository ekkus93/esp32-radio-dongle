# ESP32 Radio Dongle V1 Hardware

## Selected prototype boards

V1 uses these verified development boards for initial bring-up:

- **ESP32-S3:** AYWHP ESP32-S3-DevKitC-1-N16R8, ASIN `B0DG8L5NG5`.
- **ESP32-WROOM-32:** Aideepen 30-pin ESP-WROOM-32 development board, ASIN `B0BQJ8BTVB`.

Board-selection evidence and substitution caveats are recorded in `docs/V1_BOARD_VERIFICATION.md`.

The electrical-design/preflight evidence for V1-103 is recorded in `docs/V1_ELECTRICAL_PREFLIGHT.md`.

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

## V1-103 electrical preflight result

The datasheet/source audit for the direct GPIO link is complete:

- both sides use nominal 3.3 V I/O domains;
- the documented DC input/output levels are mutually compatible;
- no level shifter is required while both boards are normally powered;
- S3 GPIO4/5/6/7 are not ESP32-S3 strapping pins;
- WROOM GPIO16/17/25/26 are not original-ESP32 strapping pins;
- production firmware maps every driven signal output-to-input rather than output-to-output; and
- both production paths enable hardware RTS/CTS.

The detailed voltage-margin and pin audit is in `docs/V1_ELECTRICAL_PREFLIGHT.md`.

The static preflight is **PASS**, but V1-103 remains open until the common-ground/power arrangement is actually observed on the physical boards.

## UART routing implementation note

The WROOM firmware owns UART2 rather than depending on fixed HCI-UART pin defaults. It explicitly routes UART2 as:

```text
TX  = GPIO17
RX  = GPIO16
RTS = GPIO26
CTS = GPIO25
```

ESP32 GPIO Matrix routing makes the chosen CTS/RTS assignment valid even though GPIO25/26 are not inherently fixed UART2 flow-control pins.

The S3 firmware explicitly routes UART1 as:

```text
TX  = GPIO4
RX  = GPIO5
RTS = GPIO6
CTS = GPIO7
```

The WROOM Bluetooth controller is exposed to application bridge code through VHCI, while the application provides the external H4 UART transport. This lets the project enforce the reference pinout, bounded framing, queues, diagnostics, and recovery semantics.

## Prototype power

For the two-development-board prototype:

1. Give the ESP32-S3 development board one normal board power source.
2. Give the ESP32-WROOM-32 development board one normal board power source.
3. Connect the two board grounds.
4. **Do not connect the two boards' regulated 3.3 V output rails together.**
5. **Do not connect the two boards' 5 V/VBUS rails together merely to share a reference.**
6. Connect only the HCI TX/RX/RTS/CTS signals and common ground shown above unless a later documented revision says otherwise.
7. Do not simultaneously feed a development board from USB and a separate header power input unless that specific board documentation explicitly permits that arrangement.

Both selected MCU boards use 3.3 V GPIO logic, so the reference UART connection does not require a logic-level translator while both boards are powered normally.

### Important: do not drive an unpowered peer

The direct GPIO link is **not** qualified for one MCU board being fully unpowered while the other board actively drives TX or RTS into it.

GPIO DC limits are referenced to the receiving I/O-domain VDD. Therefore V1 prototype testing must not assume power-off tolerance merely because both boards use 3.3 V when powered.

For the prototype:

- keep both MCU boards powered whenever the four HCI signal wires are active;
- normal MCU reset testing is acceptable while both boards remain powered;
- do not use an asymmetric WROOM-first/S3-second cold-power sequence with active HCI signal wiring as an acceptance test; and
- if future hardware must tolerate independent power removal with signals still attached, add and qualify explicit isolation/sequencing/protection.

## Development USB usage

During bring-up it is normal to have two host cables:

```text
PC USB #1 -> ESP32-S3 native USB       (eventual product-facing USB)
PC USB #2 -> WROOM CP210x USB-UART     (development flash/log only)
```

The WROOM USB-UART path is not part of the final Bluetooth data path. UART0 remains available for development flashing/logging where practical.

When performing the first direct-GPIO electrical test, use a power arrangement that brings both boards into their normal powered state before exercising the HCI link. The detailed bench sequence is in `docs/V1_UART_BRINGUP.md`.

## Hardware validation still required

Board identity, pin/native-USB compatibility, and static electrical compatibility are already verified. V1-G10 remains open only for the physical behavior that has intentionally been deferred:

- connect and verify common ground;
- confirm the intended per-board power arrangement on the bench;
- verify S3 -> WROOM UART traffic;
- verify WROOM -> S3 UART traffic;
- verify both RTS/CTS crossings assert, throttle, and release correctly;
- verify traffic resumes without corruption after backpressure;
- verify reset/boot behavior with the wiring attached; and
- confirm no physical board-specific reset/bootstrap issue appears in the assembled prototype.

The dedicated smoke firmware and bench procedure for these tests are documented in `docs/V1_UART_BRINGUP.md`.
