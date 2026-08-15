# ESP32 Radio Dongle V1 Evidence Index

This index separates software evidence from physical-device/host acceptance evidence.

## Evidence policy

A task that requires physical boards, USB enumeration, Bluetooth peripherals, Windows, or Linux host behavior remains open until that observation is actually performed. Source code, CI compilation, or a test harness does not substitute for physical evidence.

Device testing is currently deferred by project decision. Deferred device tasks are neither PASS nor FAIL.

## Software-validated gates

### V1-G00 — Repository/toolchain baseline: PASS

Evidence recorded in `ESP32_RADIO_DONGLE_V1_TODO.md`:

- ESP-IDF pinned to v5.5.5.
- GitHub Actions run `31851995899`.
- Both production firmware targets built successfully.
- Host H4 regression suite passed.

Relevant implementation/docs:

- `.github/workflows/firmware-ci.yml`
- `docs/BUILDING.md`
- `firmware/esp32_wroom_bt_controller/`
- `firmware/esp32s3_usb_bridge/`

### V1-G20 — HCI H4 transport core: PASS

Evidence:

- bounded parser and packet model in `firmware/components/radio_h4/`;
- fixed-capacity queue behavior covered by host tests;
- valid command/event/ACL/SCO-model packet coverage;
- fragmented and back-to-back input coverage;
- invalid type, oversized, truncated, trailing-data, and queue-exhaustion coverage; and
- strict host compilation with `-Wall -Wextra -Werror -pedantic` in CI.

Relevant test:

- `tests/host/test_radio_h4.c`

## Software implementation complete but physical gate still open

### V1-G10 — Hardware contract and UART bring-up

Software preparation complete:

- board compatibility guide: `docs/V1_BOARD_VERIFICATION.md`;
- dedicated WROOM smoke firmware: `firmware/bringup/esp32_wroom_uart_smoke/`;
- dedicated S3 smoke firmware: `firmware/bringup/esp32s3_uart_smoke/`;
- shared bidirectional UART/RTS-CTS test logic: `firmware/components/radio_uart_smoke/`;
- bench procedure: `docs/V1_UART_BRINGUP.md`; and
- evidence template: `docs/V1_UART_BRINGUP_EVIDENCE.md`.

Still required when device testing resumes:

- exact physical board identities;
- five-wire connection/power evidence;
- bidirectional UART pass;
- both RTS/CTS crossings under measured backpressure; and
- reset/boot-state checks.

### V1-G30 — WROOM Bluetooth controller

Software implementation complete:

- original ESP32 target;
- BR/EDR + BLE controller-only configuration;
- VHCI bridge;
- UART2 GPIO mapping and hardware flow control;
- bounded queues/parsing; and
- fail-closed restart semantics.

Still required:

- raw HCI command/event exchange on hardware;
- controller version/capability evidence from the actual WROOM; and
- reset/restart scenarios.

### V1-G40 — S3 USB Bluetooth device

Software implementation complete:

- ESP32-S3 native USB target;
- standard Bluetooth Wireless Controller class identity;
- command/event/ACL endpoints and control path;
- stable serial generation policy;
- no fake SCO/ISO endpoints; and
- lifecycle source handling for attach/detach/suspend/resume.

Still required:

- physical USB descriptor capture;
- host enumeration validation; and
- real reset/suspend/resume behavior.

### V1-G50 — Integrated USB-to-HCI bridge

Software path exists end-to-end in source.

Still required:

- a real USB -> S3 -> UART -> WROOM -> UART -> S3 -> USB HCI round trip; and
- WROOM-loss behavior after USB enumeration.

## Fully device-deferred gate groups

The following gate groups fundamentally require hardware and/or real host Bluetooth stacks and remain open:

- V1-G60 — Linux driverless bring-up;
- V1-G70 — Windows driverless bring-up;
- V1-G80 — recovery/robustness;
- V1-G90 — performance/stability; and
- V1-G100 — Bluetooth compatibility matrix.

## Release/security software evidence

### V1-G110 — Security, identity, and release hygiene

Static review evidence already exists in:

- `docs/V1_SECURITY_REVIEW.md`;
- `docs/V1_RELEASE_CONFIGURATION.md`;
- `scripts/check-release-logging.sh`;
- `firmware/esp32_wroom_bt_controller/sdkconfig.release`; and
- `firmware/esp32s3_usb_bridge/sdkconfig.release`.

Release CI is configured to build both production targets with WARN-only maximum/default logging, verify the generated configuration, hash all binary flash inputs, and upload commit-addressed artifacts.

Do not mark V1-G110 PASS until the release-profile CI jobs have actually passed for the relevant repository state.

## Documentation gate

### V1-G120 — Documentation/user experience

Software documentation exists for build, flashing, wiring, usage, troubleshooting, limitations, board verification, UART bring-up, security review, and release configuration.

The gate remains open because the complete instructions must eventually be validated against real boards and clean Windows/Linux hosts.

## Final acceptance

V1-GFINAL remains open. Software-only work can prepare release artifacts, checks, documentation, and test harnesses, but cannot establish the central product claim: plug the S3 native USB connection into Windows or Linux and use Bluetooth Classic + BLE through the operating system's normal Bluetooth stack without project-specific host software.
