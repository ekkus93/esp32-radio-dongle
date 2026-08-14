# Using ESP32 Radio Dongle V1

V1 is designed to behave like a normal USB Bluetooth adapter. Normal operation must not require an ESP32 Radio Dongle host utility, serial attach command, custom daemon, custom Bluetooth driver, or project-specific INF.

## Before normal use

Both firmware images must already be flashed and the two boards must be wired according to `docs/HARDWARE.md`.

Connect the computer to the **ESP32-S3 native USB device port**. The WROOM development USB-UART port is not part of the normal Bluetooth data path.

## Windows

The intended V1 experience is:

1. plug the S3 native USB connection into the Windows computer;
2. allow Windows to enumerate the USB Bluetooth controller using its normal in-box Bluetooth support;
3. open the normal **Bluetooth & devices** settings;
4. scan, pair, connect, and use devices through the Windows Bluetooth UI exactly as with a conventional USB Bluetooth adapter.

Do not install a project-specific driver or INF to make a failed V1 prototype appear successful. Driverless binding is an acceptance requirement.

For acceptance evidence, record the Windows version/build, the driver that actually bound, and representative BLE and Classic devices tested.

## Linux

The intended V1 experience is:

1. plug the S3 native USB connection into the Linux computer;
2. allow the kernel's normal USB Bluetooth support to bind;
3. allow BlueZ to discover the new HCI controller;
4. use the desktop Bluetooth UI or ordinary BlueZ tooling such as `bluetoothctl`.

Useful diagnostic commands during acceptance include:

```sh
lsusb
lsusb -v
journalctl -k -b | grep -Ei 'usb|bluetooth|btusb|hci'
bluetoothctl list
bluetoothctl show
```

These are normal OS diagnostic tools; they are not required helper software for ordinary V1 operation.

Do **not** use `btattach` as part of normal acceptance. The host must see the S3 as a USB Bluetooth controller, not as a serial HCI transport needing manual attachment.

## Expected capabilities

V1 targets:

- Bluetooth LE scanning, connection, pairing/bonding where applicable, and normal GATT use through the host stack;
- Bluetooth Classic discovery, pairing, and ACL-based profiles;
- representative Classic HID testing; and
- representative sustained ACL traffic such as A2DP where the host/device combination permits it.

## V1 synchronous-audio limitation

V1 intentionally does **not** expose SCO/eSCO synchronous voice transport. HFP/HSP voice audio is therefore outside the V1 release scope.

This is not a general Bluetooth Classic disablement. Classic profiles carried over ACL remain in scope.

See `docs/USB_BLUETOOTH_V1.md` and `docs/LIMITATIONS.md` for the rationale and release status.

## Development diagnostics

Firmware diagnostics are intended to be collected from development logging connections or build/test output. They must not require replacing the standard host Bluetooth path with a serial helper.

Useful counters include:

- HCI packets by type and direction;
- malformed H4 packets;
- UART errors;
- queue high-water and queue-full counts;
- USB attach/detach/suspend/resume events; and
- controlled recovery counts.

## V2

Wi-Fi is not required for V1. Future Wi-Fi work must preserve the V1 driverless Bluetooth behavior rather than turning normal Bluetooth operation into a host-software-dependent workflow.
