# ESP32 Radio Dongle V1 Known Limitations

This file records intentional V1 limits separately from unresolved defects. It must be updated with actual host/peripheral test evidence before V1 release.

## Current validation boundary

Physical ESP32 and host-device testing is intentionally deferred at the current development checkpoint.

The repository has software/CI evidence for H4 framing and queues, production USB Bluetooth class behavior under a fake TinyUSB backend, exact production USB descriptor bytes/strings, logging/component policy, and ESP-IDF compilation/release builds where recorded in the evidence index. Those checks do **not** establish any of the following physical claims:

- electrical UART or RTS/CTS correctness on the selected boards;
- successful native-USB enumeration of the V1 Bluetooth firmware from a real ESP32-S3;
- automatic Windows Bluetooth-driver binding;
- automatic Linux `btusb`/BlueZ binding;
- real BR/EDR or BLE radio operation through the WROOM controller;
- peripheral pairing/interoperability;
- suspend/resume or reset recovery; or
- sustained throughput/stability.

Those items remain open until device testing resumes. See `V1_EVIDENCE_INDEX.md` for the separation between software evidence and deferred device evidence.

## SCO/eSCO synchronous voice transport

**Status: intentionally not supported in V1.**

The pinned TinyUSB Bluetooth device implementation does not provide the end-to-end SCO/ISO application path required by this project. V1 therefore uses a project-owned TinyUSB application class for HCI command/event/ACL data.

The host-visible configuration still uses the legacy Bluetooth Controller two-interface shape:

- interface 0 carries HCI command/event/ACL traffic; and
- interface 1 alternate setting 0 has zero endpoints and represents zero active voice channels.

V1 exposes no nonzero-bandwidth SCO alternate settings or usable Bluetooth voice isochronous endpoints. The WROOM controller is configured for zero synchronous BR/EDR connections. V1 does not claim HFP/HSP voice-audio support.

Bluetooth Classic itself remains in scope. ACL-based Classic profiles, including representative HID and A2DP testing, are release targets.

## USB identity

Development builds currently use placeholder VID/PID `0xCAFE:0x4011`.

This is a development identity only. A distributed production/release build must use a USB VID/PID the project is authorized to use. The project must not ship under another manufacturer's assigned identity without authorization.

Production USB identity authorization has not yet been completed and remains a release/commercialization requirement.

## UART rate

The shared HCI UART software baseline is **115200 baud**.

This is deliberately conservative. The final V1 value is not selected until physical RTS/CTS and sustained bidirectional traffic testing is complete. The S3 and WROOM use one shared configuration definition so they cannot intentionally diverge in source.

## Throughput

No final V1 throughput claim has been established. USB Full Speed, UART transport, HCI framing, controller buffering, and real Bluetooth traffic all contribute to the final usable rate.

Performance numbers must be measured on the assembled hardware rather than inferred from nominal Bluetooth PHY rates.

## Development-board status

The exact initial V1 development boards **are identified and approved for the reference pin contract**:

- AYWHP ESP32-S3-DevKitC-1-N16R8, ASIN `B0DG8L5NG5`.
- Aideepen 30-pin ESP-WROOM-32, ASIN `B0BQJ8BTVB`.

`docs/V1_BOARD_VERIFICATION.md` records the purchase-listing, prior-photo/USB-enumeration, and pin/native-USB evidence used to close V1-102.

The remaining limitation is physical validation, not board identity: the five-wire interconnect, common ground, bidirectional UART, real RTS/CTS backpressure, and reset/boot behavior have not yet been exercised for this project checkpoint.

If different development boards are substituted, their pin/native-USB compatibility must be rechecked.

## Host operating systems

The architecture targets standard Windows and Linux USB Bluetooth stacks, but final tested Windows versions/builds and Linux distributions/kernel/BlueZ versions remain to be recorded from physical acceptance runs.

## Peripheral compatibility

The final tested BLE, Classic HID, and Classic audio/ACL peripheral matrix remains to be populated from hardware acceptance runs.

## Recovery and stability

Real unplug/replug, host reboot, suspend/resume, S3/WROOM reset, queue-pressure, logging-load, and multi-hour stability behavior remain unqualified until device testing resumes.

## Wi-Fi

Wi-Fi is explicitly **not part of V1**.

V2 may investigate adding ESP32-S3 Wi-Fi functionality, including possible RTL8188EU-style USB compatibility experiments. No V2 Wi-Fi work may weaken V1's requirement for normal driverless Bluetooth operation on Windows and Linux.
