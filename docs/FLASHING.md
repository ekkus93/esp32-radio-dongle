# Flashing ESP32 Radio Dongle V1

V1 contains two independent firmware images. Flash the ESP32-WROOM-32 controller firmware and the ESP32-S3 USB bridge firmware separately during development.

## Prerequisites

Use ESP-IDF **v5.5.5 exactly** and complete the build steps in `docs/BUILDING.md` first.

## Flash the ESP32-WROOM-32

Connect the WROOM development board's normal USB-UART programming connector to the development computer.

From the repository root:

```sh
cd firmware/esp32_wroom_bt_controller
idf.py -DIDF_TARGET=esp32 build
idf.py -p /dev/ttyUSB0 flash monitor
```

Replace `/dev/ttyUSB0` with the actual programming port. On Windows use the appropriate `COMx` name.

The WROOM USB-UART connection is a **development programming/logging connection only**. It is not the final host-facing Bluetooth connection.

Expected early boot diagnostics include the pinned ESP-IDF version, WROOM bridge startup, Bluetooth controller initialization, and the HCI UART pin/rate configuration.

## Flash the ESP32-S3

Use the programming method appropriate for the exact S3 development board. Many S3 boards can flash through their native USB connector; some expose a separate USB-UART connector as well.

From the repository root:

```sh
cd firmware/esp32s3_usb_bridge
idf.py -DIDF_TARGET=esp32s3 build
idf.py -p /dev/ttyACM0 flash monitor
```

Replace `/dev/ttyACM0` with the actual programming port. On Windows use the appropriate `COMx` name.

If the same native-USB connector is used for flashing and for V1 Bluetooth operation, stop the serial monitor after flashing and reconnect/reset the board as required by that board so the firmware can enumerate as the Bluetooth device.

## Final host-facing USB connector

The final V1 host connection is **the ESP32-S3 native USB device connection**.

```text
Windows/Linux
     |
     | USB
     v
ESP32-S3 native USB
     |
     | HCI H4 UART + RTS/CTS
     v
ESP32-WROOM-32
     |
     +--> Bluetooth Classic + BLE radio
```

A connector that reaches only a CP210x, CH340, FTDI, or other USB-UART bridge is not the host-facing V1 Bluetooth connection.

## Development power arrangement

During two-board bring-up it is acceptable to power each development board from its own USB connection while sharing ground and the documented HCI signals.

Do **not** connect the two independently regulated 3.3 V output rails together. See `docs/HARDWARE.md` for the reference wiring and power rules.

## Flash order

For initial bring-up, use this order:

1. flash and boot the WROOM firmware;
2. verify its controller/transport startup diagnostics;
3. wire the documented HCI UART link;
4. flash/reset the S3 firmware;
5. the S3 sends HCI Reset and Read Local Version Information to the WROOM before it intentionally installs/attaches its USB Bluetooth device;
6. only after the controller probe succeeds should the host see the USB Bluetooth device.

If the WROOM is absent or the HCI probe fails, the S3 intentionally does not proceed as if a usable Bluetooth controller exists.

## Erasing generated state

When troubleshooting a configuration mismatch, rebuild from a clean state rather than carrying an old `sdkconfig` forward:

```sh
rm -f firmware/esp32_wroom_bt_controller/sdkconfig
rm -rf firmware/esp32_wroom_bt_controller/build
rm -f firmware/esp32s3_usb_bridge/sdkconfig
rm -rf firmware/esp32s3_usb_bridge/build
```

Then rebuild and flash both targets using ESP-IDF v5.5.5.
