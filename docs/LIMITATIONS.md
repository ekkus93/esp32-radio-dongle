# ESP32 Radio Dongle V1 Known Limitations

This file records intentional V1 limits separately from unresolved defects. It must be updated with actual host/peripheral test evidence before V1 release.

## SCO/eSCO synchronous voice transport

**Status: intentionally not supported in V1.**

The pinned TinyUSB Bluetooth device implementation contains voice/ISO endpoint scaffolding but does not provide a reliable end-to-end application data path suitable for this project. V1 therefore exposes only Bluetooth HCI command, event, and ACL USB transports through a small project-owned TinyUSB application class.

The WROOM controller is configured for zero synchronous BR/EDR connections, and the S3 descriptor set exposes no Bluetooth voice isochronous endpoints. V1 does not claim HFP/HSP voice-audio support.

Bluetooth Classic itself remains in scope. ACL-based Classic profiles, including representative HID and A2DP testing, are release targets.

## USB identity

Development builds currently use placeholder VID/PID `0xCAFE:0x4011`.

This is a development identity only. A distributed production/release build must use a USB VID/PID the project is authorized to use. The project must not ship under another manufacturer's assigned identity without authorization.

## UART rate

The initial shared HCI UART baud is **115200 baud**.

This is deliberately conservative. The final V1 value is not selected until physical RTS/CTS and sustained bidirectional traffic testing is complete. The S3 and WROOM use one shared configuration definition so they cannot intentionally diverge in source.

## Throughput

No final V1 throughput claim has been established. USB Full Speed, UART transport, HCI framing, controller buffering, and real Bluetooth traffic all contribute to the final usable rate.

Performance numbers must be measured on the assembled hardware rather than inferred from nominal Bluetooth PHY rates.

## Development-board compatibility

The reference GPIO assignment is documented, but the exact ESP32-S3 and ESP32-WROOM-32 development-board models for initial physical acceptance have not yet been recorded.

Before hardware bring-up closes, verify that the selected S3 board exposes native USB and makes GPIO4-7 usable, and that the selected WROOM board exposes GPIO16/17/25/26 without board-specific conflicts.

## Host operating systems

The architecture targets standard Windows and Linux USB Bluetooth stacks, but final tested Windows versions/builds and Linux distributions/kernel/BlueZ versions remain to be recorded from physical acceptance runs.

## Peripheral compatibility

The final tested BLE, Classic HID, and Classic audio/ACL peripheral matrix remains to be populated from hardware acceptance runs.

## Wi-Fi

Wi-Fi is explicitly **not part of V1**.

V2 may investigate adding ESP32-S3 Wi-Fi functionality, including possible RTL8188EU-style USB compatibility experiments. No V2 Wi-Fi work may weaken V1's requirement for normal driverless Bluetooth operation on Windows and Linux.
