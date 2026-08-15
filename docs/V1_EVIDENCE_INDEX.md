# ESP32 Radio Dongle V1 Evidence Index

This index separates software evidence from physical-device/host acceptance evidence.

## Evidence policy

A task that requires physical boards, USB enumeration, Bluetooth peripherals, Windows, or Linux host behavior remains open until that observation is actually performed. Source code, CI compilation, or a test harness does not substitute for physical evidence.

Device testing is currently deferred by project decision. Deferred device tasks are neither PASS nor FAIL.

## Current software/documentation checkpoint

The fully green firmware/software checkpoint is GitHub Actions run `31873127842` at commit `57864e9e83b5760c70fbff2e3b5f1ab1cfbe174e`.

The subsequent V1-1305 documentation audit reconciles the SPEC/TODO/supporting docs without changing the underlying device-test status. See `docs/V1_DOCUMENTATION_EVIDENCE_AUDIT.md`.

## Software-validated gates

### V1-G00 — Repository/toolchain baseline: PASS

Evidence:

- ESP-IDF pinned to v5.5.5.
- both production firmware targets build independently;
- target/version guards are present;
- GitHub Actions build coverage exists; and
- host H4 regression suite passes.

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

Board-selection evidence is complete:

- V1-102 is PASS in `docs/V1_BOARD_VERIFICATION.md`.
- Selected S3: AYWHP ESP32-S3-DevKitC-1-N16R8, ASIN `B0DG8L5NG5`.
- Selected WROOM: Aideepen 30-pin ESP-WROOM-32, ASIN `B0BQJ8BTVB`.
- Prior project-owner board photographs and Linux USB enumeration were used as physical identity evidence.
- The S3 previously enumerated project USB firmware as `303a:4001`.
- The WROOM board previously enumerated its CP210x USB-UART bridge as `10c4:ea60`.
- Pin/native-USB compatibility is documented in `docs/V1_BOARD_VERIFICATION.md` and `docs/HARDWARE.md`.

V1-103 static/datasheet electrical preflight is complete:

- `docs/V1_ELECTRICAL_PREFLIGHT.md` records the detailed result.
- Both MCU I/O domains are nominally 3.3 V and their documented DC logic levels are mutually compatible.
- At 3.3 V, the conservative documented high-level margin is 0.165 V and low-level margin is 0.495 V.
- S3 GPIO4/5/6/7 do not overlap the ESP32-S3 strapping pins GPIO0/3/45/46.
- WROOM GPIO16/17/25/26 do not overlap the original-ESP32 strapping pins GPIO0/2/5/12/15.
- Production and smoke firmware configure TX->RX and RTS->CTS with no intentional output-to-output crossing.
- `scripts/check-electrical-contract.sh` enforces the pin/direction/flow-control source contract in Firmware CI.
- The prototype power policy explicitly forbids tying the independent 3.3 V or 5 V/VBUS rails together.
- The direct GPIO link is not claimed to tolerate one MCU board being fully unpowered while the peer actively drives TX/RTS; the prior asymmetric cold-power test was removed.

Software preparation complete:

- dedicated WROOM smoke firmware: `firmware/bringup/esp32_wroom_uart_smoke/`;
- dedicated S3 smoke firmware: `firmware/bringup/esp32s3_uart_smoke/`;
- shared bidirectional UART/RTS-CTS test logic: `firmware/components/radio_uart_smoke/`;
- electrical preflight: `docs/V1_ELECTRICAL_PREFLIGHT.md`;
- bench procedure: `docs/V1_UART_BRINGUP.md`; and
- evidence template: `docs/V1_UART_BRINGUP_EVIDENCE.md`.

Still required when device testing resumes:

- physical common-ground/power-arrangement evidence for V1-103;
- five-wire connection evidence;
- bidirectional UART pass;
- both RTS/CTS crossings under measured backpressure; and
- reset/boot-state checks with both MCU boards powered normally.

### V1-G30 — WROOM Bluetooth controller

Software implementation complete:

- original ESP32 target;
- ESP-IDF v5.5.5;
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
- standard Bluetooth Wireless Controller E0/01/01 identity;
- two-interface legacy Bluetooth Controller USB configuration;
- interface 0 carries command/event/ACL transport;
- interface 1 alternate setting 0 is endpoint-free and represents zero active voice channels;
- stable serial generation policy;
- no usable/fake SCO/ISO endpoints or nonzero-bandwidth alternate settings; and
- lifecycle source handling for attach/detach/suspend/resume.

The current normative layout is documented directly in `docs/ESP32_RADIO_DONGLE_V1_SPEC.md` and `docs/USB_BLUETOOTH_V1.md`. `docs/V1_USB_COMPLIANCE_CORRECTION.md` remains as decision history.

Host-only evidence from GitHub Actions run `31873127842`:

- formatting, release-log policy, and component-boundary policy pass;
- strict H4 host tests pass;
- the actual production `radio_usb_bth.c` compiles and passes behavior tests against a minimal fake TinyUSB backend;
- class registration and primary-interface descriptor-open rejection paths are exercised;
- the empty second Bluetooth interface is accepted only after the primary interface, only as the adjacent interface number, only at alternate setting 0, and only with zero endpoints;
- HCI command class-control transfers are exercised;
- legacy single-function device-targeted HCI class requests, including `bRequest=0xE0`, are exercised;
- device-to-host, non-class, misrouted interface, and oversized HCI control requests are rejected;
- fragmented ACL OUT reassembly is exercised;
- incomplete, oversized, and ambiguous zero-length ACL transfers fail closed;
- event packet validation and completion callbacks are exercised;
- ACL IN exact-full-speed-packet transfer termination via ZLP is exercised;
- transfer-error/reset behavior is exercised;
- the actual production `usb_descriptors.c` is inspected byte-for-byte by a host test;
- configuration total length is verified as 48 bytes with `bNumInterfaces=2`;
- interface 0 event IN, ACL OUT, and ACL IN endpoint address/type/size contracts are verified;
- interface 1 alternate setting 0 is verified as E0/01/01 with zero endpoints;
- development VID/PID `CAFE:4011`, strings, and one-configuration layout are verified; and
- deterministic factory-MAC serial formatting plus the explicit fallback serial are verified.

Relevant host tests:

- `tests/host/test_radio_usb_bth.c`
- `tests/host/test_radio_usb_bth_control_compat.c`
- `tests/host/test_usb_descriptors.c`

Still required:

- physical USB descriptor capture;
- host enumeration validation; and
- real reset/suspend/resume behavior.

### V1-G50 — Integrated USB-to-HCI bridge

Software path exists end-to-end in source.

Software evidence includes:

- command/event/ACL forwarding paths;
- controller readiness probe using HCI Reset and Read Local Version;
- bounded queues and H4 validation;
- no fabricated success responses; and
- development diagnostic counters.

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

### V1-G110 — Security, identity, and release hygiene: software configuration PASS

Static review evidence exists in:

- `docs/V1_SECURITY_REVIEW.md`;
- `docs/V1_RELEASE_CONFIGURATION.md`;
- `scripts/check-release-logging.sh`;
- `scripts/check-component-boundaries.sh`;
- `firmware/esp32_wroom_bt_controller/sdkconfig.release`; and
- `firmware/esp32s3_usb_bridge/sdkconfig.release`.

GitHub Actions run `31873127842`, commit `57864e9e83b5760c70fbff2e3b5f1ab1cfbe174e`, is the fully green software checkpoint:

- host formatting and policy gates passed;
- all H4/USB class/control-compatibility/descriptor host tests passed;
- normal ESP32-WROOM-32 controller build passed;
- normal ESP32-S3 USB bridge build passed;
- both dedicated UART smoke-image builds passed;
- both WARN-only production release-profile builds passed; and
- both release flash-input artifacts were uploaded.

Development release artifacts from that run:

- `v1-release-esp32-controller-57864e9e83b5760c70fbff2e3b5f1ab1cfbe174e`
  - artifact archive digest: `sha256:a4d9e6559a3f42886395e3430e4b9574b6e2d81e38165c6ba01dfffa253e9b88`
- `v1-release-esp32s3-bridge-57864e9e83b5760c70fbff2e3b5f1ab1cfbe174e`
  - artifact archive digest: `sha256:ef88512964d3bac8de6e2c60d37912cf1b062cd80118cc4564c0aae41aa40cae`

Those artifacts are pipeline/software evidence only; they are not the final V1 release because hardware qualification has not occurred. The final release must be generated from the exact hardware-qualified release commit.

V1-904 remains open: only real sustained traffic can establish whether development logging affects timing on physical hardware.

## Documentation/evidence audit

### V1-1305 — software audit: PASS; hardware-behavior re-audit OPEN

Audit record:

- `docs/V1_DOCUMENTATION_EVIDENCE_AUDIT.md`

Audit conclusions:

- completed software tasks/gates have traceable implementation/test/CI/documentation evidence;
- the main SPEC/TODO are reconciled with the implemented v5.5.5, board-selection, two-interface USB, no-SCO, release, and development-boundary decisions;
- stale board-status and release-review wording was corrected in supporting docs;
- no known open V1 blocker is hidden by a development-only host path or bring-up firmware;
- `scripts/check-doc-contract.sh` and `.github/workflows/docs-ci.yml` enforce the locked documentation contract; and
- the parent V1-1305 remains open only for the required post-device-test reconciliation against measured hardware behavior.

## Documentation gate

### V1-G120 — Documentation/user experience

Software documentation exists for build, flashing, selected boards/wiring, electrical preflight, usage, troubleshooting, limitations, UART bring-up, security review, release configuration, USB transport, evidence mapping, and the V1-1305 audit.

The gate remains open because the complete instructions must eventually be validated against real boards and clean Windows/Linux hosts.

## Final acceptance

V1-GFINAL remains open. Software/documentation work can prepare release artifacts, checks, documentation, and test harnesses, but cannot establish the central product claim: plug the S3 native USB connection into Windows or Linux and use Bluetooth Classic + BLE through the operating system's normal Bluetooth stack without project-specific host software.
