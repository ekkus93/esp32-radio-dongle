# ESP32 Radio Dongle

ESP32 Radio Dongle is a two-MCU USB radio-adapter project.

## V1 goal

V1 turns an **ESP32-S3 + original ESP32-WROOM-32** pair into a normal USB Bluetooth Classic + Bluetooth LE adapter for Windows and Linux.

The defining V1 requirement is **no project-specific host-side software for normal Bluetooth operation**. The ESP32-S3 must enumerate through its native USB peripheral as a standard USB Bluetooth controller so Windows/Linux can use their normal Bluetooth stacks.

```text
Windows / Linux
      |
      | standard USB Bluetooth HCI
      v
+-------------------+
| ESP32-S3          |
| native USB bridge |
+-------------------+
      |
      | HCI H4 UART + RTS/CTS
      v
+-------------------+
| ESP32-WROOM-32    |
| Classic + BLE     |
| controller/radio  |
+-------------------+
```

Wi-Fi is explicitly deferred to V2.

## Selected V1 development boards

The initial V1 boards are identified and approved for the reference pin/native-USB contract:

- AYWHP ESP32-S3-DevKitC-1-N16R8 — ASIN `B0DG8L5NG5`.
- Aideepen 30-pin ESP-WROOM-32 — ASIN `B0BQJ8BTVB`.

See `docs/V1_BOARD_VERIFICATION.md` for the evidence and substitution caveats.

## Reference wiring

| ESP32-S3 | Direction | ESP32-WROOM-32 |
|---|:---:|---|
| GPIO4 TX | -> | GPIO16 RX2 |
| GPIO5 RX | <- | GPIO17 TX2 |
| GPIO6 RTS | -> | GPIO25 CTS |
| GPIO7 CTS | <- | GPIO26 RTS |
| GND | <-> | GND |

See `docs/HARDWARE.md` before wiring or powering two development boards together.

## Toolchain

Both firmware targets are pinned to **ESP-IDF v5.5.5**.

- `firmware/esp32_wroom_bt_controller/` — original ESP32 dual-mode Bluetooth controller bridge.
- `firmware/esp32s3_usb_bridge/` — ESP32-S3 native USB Bluetooth bridge.
- `firmware/components/radio_h4/` — shared bounded HCI H4 framing/configuration.

Development-only UART smoke images live under `firmware/bringup/` and are not release firmware.

## Documentation

- `docs/ESP32_RADIO_DONGLE_V1_SPEC.md` — current V1 design and acceptance contract.
- `docs/ESP32_RADIO_DONGLE_V1_TODO.md` — implementation/verification tracker.
- `docs/V1_EVIDENCE_INDEX.md` — software/device evidence mapping.
- `docs/V1_DOCUMENTATION_EVIDENCE_AUDIT.md` — V1-1305 software documentation/evidence audit.
- `docs/V1_BOARD_VERIFICATION.md` — selected board and pin/native-USB evidence.
- `docs/BUILDING.md` — pinned toolchain and reproducible builds.
- `docs/FLASHING.md` — flashing both MCUs.
- `docs/HARDWARE.md` — wiring and prototype power rules.
- `docs/USB_BLUETOOTH_V1.md` — host-visible USB Bluetooth contract.
- `docs/USAGE.md` — intended Windows/Linux normal use.
- `docs/TROUBLESHOOTING.md` — bring-up and diagnostic guidance.
- `docs/LIMITATIONS.md` — known V1 limitations and pending acceptance evidence.
- `docs/V1_RELEASE_CONFIGURATION.md` — production/release build and logging policy.
- `docs/V1_SECURITY_REVIEW.md` — software input/failure-semantics review.

## Current V1 scope note

V1 intentionally omits SCO/eSCO synchronous voice transport. The USB function uses the legacy two-interface Bluetooth Controller layout: interface 0 carries command/event/ACL transport, while interface 1 alternate setting 0 has zero endpoints for zero active voice bandwidth. The WROOM is configured for zero synchronous BR/EDR connections.

Bluetooth Classic ACL profiles remain in scope, including representative HID and A2DP testing.

## Validation status

Software/CI evidence covers H4 framing/queues, the production USB class and descriptors, both production firmware builds, dedicated smoke-image builds, release configuration, and development/production component separation.

Physical UART/RTS-CTS, real WROOM HCI/radio behavior, native USB enumeration, Windows/Linux driver binding, peripheral compatibility, recovery, and sustained stability remain device-acceptance work. Software evidence is not treated as a substitute for those gates.

## V2

After V1 Bluetooth is hardware-qualified, V2 may add Wi-Fi using the ESP32-S3 while preserving all V1 Bluetooth requirements. RTL8188EU-compatible USB emulation is one possible research direction, not a V1 dependency.
