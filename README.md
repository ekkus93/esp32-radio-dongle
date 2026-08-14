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

## Documentation

- `docs/ESP32_RADIO_DONGLE_V1_SPEC.md` — V1 design and acceptance contract.
- `docs/ESP32_RADIO_DONGLE_V1_TODO.md` — implementation/verification tracker.
- `docs/BUILDING.md` — pinned toolchain and reproducible builds.
- `docs/FLASHING.md` — flashing both MCUs.
- `docs/HARDWARE.md` — wiring and prototype power rules.
- `docs/USB_BLUETOOTH_V1.md` — host-visible USB Bluetooth contract.
- `docs/USAGE.md` — intended Windows/Linux normal use.
- `docs/TROUBLESHOOTING.md` — bring-up and diagnostic guidance.
- `docs/LIMITATIONS.md` — known V1 limitations and pending acceptance evidence.

## Current V1 scope note

V1 intentionally omits SCO/eSCO synchronous voice transport. The USB path exposes command/event/ACL transport and the WROOM is configured for zero synchronous BR/EDR connections. Bluetooth Classic ACL profiles remain in scope, including representative HID and A2DP testing.

## V2

After V1 Bluetooth is stable, V2 may add Wi-Fi using the ESP32-S3 while preserving all V1 Bluetooth requirements. RTL8188EU-compatible USB emulation is one possible research direction, not a V1 dependency.
