# ESP32 Radio Dongle V1 Troubleshooting

V1 troubleshooting should preserve the core acceptance rule: the completed device must use the host operating system's normal USB Bluetooth stack. Development diagnostics may use serial logging, but a host-side serial helper must not become part of normal Bluetooth operation.

## USB device does not enumerate

Check, in order:

1. The host cable is connected to an ESP32-S3 connector that reaches the **native USB peripheral**, not only a USB-UART bridge.
2. The S3 firmware was built for `esp32s3` with ESP-IDF v5.5.5.
3. The WROOM is powered, booted, and wired to the S3 according to `docs/HARDWARE.md`.
4. TX/RX and RTS/CTS are crossed exactly as documented.
5. Both boards share ground.
6. The WROOM HCI UART baud matches the S3; V1 currently uses the shared `RADIO_HCI_UART_BAUD` value.

The S3 intentionally probes the downstream controller before it installs/attaches the USB Bluetooth device. If HCI Reset or Read Local Version Information cannot complete, absence of normal USB Bluetooth enumeration can be the correct fail-closed behavior rather than a USB descriptor defect.

Collect S3 and WROOM development logs and look for the last state transition or recovery reason.

## USB enumerates but the OS does not bind Bluetooth

Do not work around this with a custom project driver during V1 acceptance.

On Linux inspect:

```sh
lsusb -v
journalctl -k -b | grep -Ei 'usb|bluetooth|btusb|hci'
```

Verify the device/interface class triple and endpoint layout described in `docs/USB_BLUETOOTH_V1.md`.

On Windows inspect Device Manager and the device/driver details. Record any error code and the driver selected by Windows. A project-specific INF is not an acceptable V1 fix.

## Bluetooth adapter appears but initialization fails

This means USB enumeration alone is not enough. Check:

- WROOM controller startup diagnostics;
- HCI Reset / Read Local Version probe diagnostics;
- malformed-H4 counters;
- UART error counters;
- queue-full counters; and
- controlled-recovery counts.

A failure must be fixed at the bridge/controller layer rather than by fabricating successful HCI responses.

## UART/HCI synchronization problems

Symptoms can include repeated S3/WROOM recovery, malformed-H4 counters, or host controller initialization failures.

Verify:

- S3 TX GPIO4 -> WROOM RX GPIO16;
- S3 RX GPIO5 <- WROOM TX GPIO17;
- S3 RTS GPIO6 -> WROOM CTS GPIO25;
- S3 CTS GPIO7 <- WROOM RTS GPIO26;
- shared ground;
- common UART baud; and
- 8 data bits, no parity, 1 stop bit, hardware RTS/CTS.

The H4 parser is fail-closed. Invalid packet types, impossible lengths, oversized packets, and incomplete frames are not intentionally allowed to bleed into a following packet.

If using a logic analyzer, capture TX/RX plus RTS/CTS so packet corruption can be distinguished from a flow-control stall.

## Pairing or connectivity problems

First determine whether the problem is USB/HCI transport or ordinary host-stack/device behavior.

Record:

- host OS/version;
- peripheral model;
- BLE versus Classic;
- whether discovery succeeds;
- whether pairing succeeds;
- whether connection succeeds;
- relevant host Bluetooth logs; and
- bridge counters before/after the failure.

Retest the same peripheral with another known-good Bluetooth adapter when practical. Host-stack or peripheral-specific behavior should be recorded separately from bridge defects.

## Sustained traffic becomes unstable

Watch:

- host-to-controller and controller-to-host queue high-water marks;
- queue-full counters;
- UART overflow/frame/parity errors;
- malformed H4 counters; and
- recovery counts.

V1 starts at a deliberately conservative UART rate. Do not increase the shared baud until the physical flow-control and sustained-traffic gates pass.

## Unexpected SCO/eSCO traffic

V1 intentionally does not support synchronous SCO/eSCO voice transport. The WROOM configuration requests zero synchronous BR/EDR connections, and the USB interface exposes no voice isochronous endpoints.

An unexpected H4 SCO packet is treated as a configuration/transport integrity problem rather than silently dropped and ignored.

## Collecting diagnostics without changing normal host requirements

Development diagnostics may be collected from:

- WROOM UART0 / its development USB-UART connector;
- an S3 board-specific debug/programming connection when available;
- GitHub Actions build/test logs; and
- ordinary Windows/Linux USB and Bluetooth diagnostics.

The diagnostic method must not require routing normal host Bluetooth traffic through a COM port or `/dev/tty*` device.

## Clean rebuild

When generated configuration may be stale:

```sh
rm -f firmware/esp32_wroom_bt_controller/sdkconfig
rm -rf firmware/esp32_wroom_bt_controller/build
rm -f firmware/esp32s3_usb_bridge/sdkconfig
rm -rf firmware/esp32s3_usb_bridge/build
```

Then rebuild both targets with ESP-IDF v5.5.5 as documented in `docs/BUILDING.md`.
