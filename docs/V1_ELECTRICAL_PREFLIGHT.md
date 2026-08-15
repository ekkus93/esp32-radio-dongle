# V1-103 Electrical Preflight Audit

Date: 2026-08-15

This document records the software/datasheet portion of **V1-103 — Verify electrical assumptions**. It deliberately does not claim that the two physical development boards have been wired or measured; those observations remain deferred until device testing resumes.

## Selected boards and interconnect

V1-102 already identifies the prototype boards:

- ESP32-S3: AYWHP ESP32-S3-DevKitC-1-N16R8, ASIN `B0DG8L5NG5`.
- ESP32-WROOM-32: Aideepen 30-pin ESP-WROOM-32 development board, ASIN `B0BQJ8BTVB`.

The V1 HCI link is:

| Function | Driving GPIO | Receiving GPIO |
| --- | --- | --- |
| S3 -> WROOM HCI TX | S3 GPIO4 | WROOM GPIO16 RX |
| WROOM -> S3 HCI TX | WROOM GPIO17 | S3 GPIO5 RX |
| S3 -> WROOM flow control | S3 GPIO6 RTS | WROOM GPIO25 CTS |
| WROOM -> S3 flow control | WROOM GPIO26 RTS | S3 GPIO7 CTS |
| Reference | S3 GND | WROOM GND |

## Logic-voltage compatibility

Espressif specifies a nominal 3.3 V GPIO/power domain for both module families.

For the ESP32-S3, the relevant 3.3 V power domains operate from 3.0 V to 3.6 V. Espressif recommends 3.3 V for normal board operation.

For ESP32-WROOM-32, `VDD33` likewise has a recommended operating range of 3.0 V to 3.6 V with 3.3 V nominal.

Both families specify, at 3.3 V:

- minimum recognized input-high voltage: `VIH = 0.75 * VDD`;
- maximum recognized input-low voltage: `VIL = 0.25 * VDD`;
- minimum guaranteed output-high voltage: `VOH = 0.8 * VDD` under the documented drive test; and
- maximum output-low voltage: `VOL = 0.1 * VDD` under the documented drive test.

At 3.3 V this gives the following conservative DC-level comparison:

```text
Input-high threshold max needed: 0.75 * 3.3 V = 2.475 V
Guaranteed output high:          0.80 * 3.3 V = 2.640 V
Static high-level margin:                         0.165 V

Input-low threshold max:         0.25 * 3.3 V = 0.825 V
Guaranteed output low:           0.10 * 3.3 V = 0.330 V
Static low-level margin:                          0.495 V
```

The direct 3.3 V UART/RTS/CTS connection is therefore electrically level-compatible. No level translator is required for the V1 prototype when both boards are powered normally.

This does **not** authorize use of 5 V logic on any HCI signal.

## Strapping/reset-pin audit

The selected HCI GPIOs do not overlap either MCU's documented strapping pins.

ESP32-S3 strapping pins are GPIO0, GPIO3, GPIO45, and GPIO46. V1 uses GPIO4, GPIO5, GPIO6, and GPIO7.

Original ESP32 strapping pins are GPIO0, GPIO2, GPIO5, MTDI/GPIO12, and MTDO/GPIO15. V1 uses GPIO16, GPIO17, GPIO25, and GPIO26.

Therefore the reference HCI wiring does not directly force either MCU's boot-mode, flash-voltage, ROM-printing, or other strapping inputs.

This is a static pin-selection result. V1-104 still requires a physical reset/reboot test with the actual wiring attached because board-level circuitry and transient behavior cannot be proven from pin numbers alone.

## Runtime signal-direction audit

The production firmware configures:

```text
ESP32-S3 UART1
  TX  = GPIO4
  RX  = GPIO5
  RTS = GPIO6
  CTS = GPIO7

ESP32-WROOM-32 UART2
  TX  = GPIO17
  RX  = GPIO16
  RTS = GPIO26
  CTS = GPIO25
```

Both use `UART_HW_FLOWCTRL_CTS_RTS`.

The resulting direct connections are output-to-input for every driven signal:

- S3 TX -> WROOM RX;
- WROOM TX -> S3 RX;
- S3 RTS -> WROOM CTS; and
- WROOM RTS -> S3 CTS.

There is no intentional output-to-output connection in the V1 reference wiring. The dedicated UART smoke firmware uses the same directional contract.

## Ground requirement

A direct single-ended UART link requires a common electrical reference. Therefore S3 GND and WROOM GND must be connected before relying on TX/RX/RTS/CTS signaling.

The physical V1-103 evidence must eventually record:

- a common-ground jumper/connection between the selected boards; and
- continuity or another unambiguous observation that both HCI endpoints share the same ground reference.

Until that observation is performed, V1-103 remains open even though the design assumption is valid.

## Prototype power-source rules

For the development-board prototype:

1. Each board receives power through **one** intended board power input.
2. The WROOM board must not be simultaneously powered from USB and a separate 5 V/3.3 V header source. Espressif documents its board power-source options as mutually exclusive.
3. Do not connect the two boards' regulated 3.3 V outputs together.
4. Do not connect their 5 V/VBUS rails together merely to create a common reference.
5. Connect GND between the boards; ground is the reference that should be shared.

The final integrated hardware may use one designed common power tree, but that is different from paralleling two development-board regulator outputs.

## Asymmetric-power hazard

The direct-GPIO link is qualified only with **both MCU I/O domains powered normally**.

Espressif's GPIO DC characteristics specify a high input range ending at approximately `VDD + 0.3 V`. When a receiving MCU is unpowered, its I/O-domain VDD is not at the normal 3.3 V operating point. Therefore this project must not assume that it is safe to leave one board completely unpowered while the other board actively drives TX or RTS into it.

For prototype testing:

- do not deliberately run the HCI link with one MCU board unpowered;
- do not use an asymmetric cold-power test with the four HCI signal wires attached as evidence of correctness;
- normal **reset** testing with both boards still powered is allowed and remains required by V1-104/V1-307;
- if independent power removal while signal wires remain attached becomes a product requirement, add/qualify appropriate isolation, sequencing, or protection rather than assuming the raw GPIO connection is power-off tolerant.

The old bring-up instruction that powered WROOM first and S3 second with all HCI wires already attached is superseded by this rule.

## Series-resistor note

Espressif recommends series resistance on application UART TX traces for EMC/harmonic control in hardware designs. The current jumper-wire prototype does not require adding series resistors merely to establish 115200-baud logic compatibility.

For a custom PCB, reserve source-side series-resistor footprints on the driven HCI signals (TX and RTS in each direction) so edge-rate/EMC tuning is possible. A resistor must not be treated as proof that an unpowered receiving GPIO is safe to drive; power-off tolerance must be separately qualified.

## Physical V1-103 checklist — deferred

When device testing resumes:

- [ ] Disconnect all USB/power before changing the permanent wiring arrangement.
- [ ] Verify the selected pin labels against `docs/HARDWARE.md`.
- [ ] Establish a common ground between S3 and WROOM.
- [ ] Ensure no 3.3 V rail is connected board-to-board.
- [ ] Ensure no 5 V/VBUS rail is connected board-to-board.
- [ ] Power both boards normally before exercising HCI signals.
- [ ] Confirm both boards remain normally powered while the direct HCI GPIO link is active.
- [ ] Record the wiring/power observation in `docs/V1_UART_BRINGUP_EVIDENCE.md`.

## V1-103 disposition

**Static/datasheet electrical preflight: PASS.**

**V1-103 parent task: OPEN** until common ground and the prototype power arrangement are physically observed on the selected boards.

## Primary references

- Espressif ESP32-S3 Hardware Design Guidelines: <https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32s3/schematic-checklist.html>
- Espressif ESP32-S3 GPIO documentation: <https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/gpio.html>
- Espressif ESP32-S3-DevKitC-1 guide: <https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/>
- Espressif ESP32-WROOM-32 datasheet: <https://documentation.espressif.com/esp32-wroom-32_datasheet_en.html>
- Espressif ESP32 hardware design guidelines: <https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32/schematic-checklist.html>
- Espressif ESP32-DevKitC guide: <https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32/esp32-devkitc/user_guide.html>
