# V1 Documentation and Evidence Audit

Date: 2026-08-15

Scope: software-auditable portion of V1-1305.

This audit reconciles the V1 specification, TODO, implementation, host tests, CI evidence, release policy, and supporting documentation. It deliberately does **not** convert deferred physical/device acceptance into software PASS results.

## Audit rule

A checked software item must have an identifiable implementation, test, policy check, build result, or documentation artifact. A task whose acceptance criterion depends on physical UART wiring, real ESP32 execution, USB enumeration, Windows/Linux driver binding, Bluetooth peripherals, suspend/resume, or sustained radio traffic remains open until that evidence is actually collected.

## Canonical V1 documents

The current V1 contract is defined by these files:

- `docs/ESP32_RADIO_DONGLE_V1_SPEC.md` — architectural and acceptance requirements.
- `docs/ESP32_RADIO_DONGLE_V1_TODO.md` — task/gate state.
- `docs/V1_EVIDENCE_INDEX.md` — evidence-to-gate mapping.
- `docs/USB_BLUETOOTH_V1.md` — detailed host-visible USB transport contract.
- `docs/V1_BOARD_VERIFICATION.md` — selected development-board evidence.
- `docs/V1_RELEASE_CONFIGURATION.md` — production/release configuration policy.
- `docs/V1_SECURITY_REVIEW.md` — software input-validation and failure-semantics review.
- `docs/LIMITATIONS.md` — intentional limits and unvalidated claims.

`docs/V1_USB_COMPLIANCE_CORRECTION.md` records the history of the USB two-interface correction. After this audit, the main SPEC/TODO are expected to contain the corrected requirements directly; the correction file remains as decision history rather than as a required override for stale normative text.

## Software evidence matrix

| Area | TODO/gate | Implementation/evidence | Audit result |
| --- | --- | --- | --- |
| Repository/toolchain | V1-001..004 / G00 | two ESP-IDF projects, v5.5.5 target guards, `.github/workflows/firmware-ci.yml`, green build evidence | PASS |
| Board selection | V1-102 | purchase listings, prior photos/USB enumeration, `V1_BOARD_VERIFICATION.md`, Espressif pin/native-USB mapping | PASS |
| H4 model/parser/queues | V1-201..203, 205..206 / G20 | `firmware/components/radio_h4/`, `tests/host/test_radio_h4.c` | PASS |
| RTS/CTS implementation | software portion of V1-204 | both production firmwares configure hardware flow control | IMPLEMENTED; physical backpressure remains open |
| WROOM controller firmware | V1-301..305 | controller-only BTDM/VHCI application, bounded H4/UART bridge, fail-closed recovery, CI build | PASS for software implementation; G30 remains physical-open |
| S3 USB bridge | V1-401..403, software portion 405 | native USB project, custom TinyUSB application class, descriptor implementation, lifecycle callbacks, CI build | PASS for software implementation; G40 remains physical-open |
| USB HCI class behavior | V1-402 / G40 software evidence | `tests/host/test_radio_usb_bth.c`, `test_radio_usb_bth_control_compat.c` | PASS |
| USB descriptor contract | V1-402/403 / G40 software evidence | `tests/host/test_usb_descriptors.c`, 48-byte two-interface configuration, E0/01/01, endpoint/string checks | PASS |
| USB-to-HCI bridge paths | V1-501..505, 507 | production S3/WROOM bridge source, bounded queues/parser, command/event/ACL handling | PASS for implementation; G50 remains physical-open |
| Readiness/recovery sequencing | software portion of V1-506 | real HCI Reset + Read Local Version probe in source; no fabricated success | PASS for implementation; WROOM-loss hardware behavior remains open |
| Input/failure security review | V1-1101..1103 | `V1_SECURITY_REVIEW.md`, bounded host tests, no fabricated completion | PASS |
| Release configuration | V1-1104 / G110 software config | WARN-only release overlays, logging policy, component-boundary policy, release CI artifacts | PASS |
| User/developer docs | V1-1201..1205 | build, flash, hardware, usage, troubleshooting docs | PASS for existence/content; G120 remains physical-walkthrough-open |
| Known limitations | software portion of V1-1206 | `LIMITATIONS.md` | PASS after stale board-status correction; host/peripheral/performance observations remain open |
| Clean build reproduction | software portion of V1-1301 | CI run 31873127842; normal + release builds; commit-addressed artifacts | PASS for pipeline; final hardware-qualified release artifacts remain open |
| V1 non-goals | V1-1304 | no Wi-Fi implementation, custom host Bluetooth driver, host helper, or production dependency on bring-up images | PASS |

## CI checkpoint audited

GitHub Actions run `31873127842` at commit `57864e9e83b5760c70fbff2e3b5f1ab1cfbe174e` is the fully green software checkpoint recorded by the project:

- formatting/policy checks passed;
- H4 host tests passed;
- production USB class tests passed;
- legacy HCI control-request compatibility tests passed;
- production descriptor tests passed;
- WROOM production build passed;
- S3 production build passed;
- both UART smoke-image builds passed;
- both WARN-only release-profile builds passed; and
- both commit-addressed release artifacts were produced.

The release artifacts from that run are pipeline evidence only; they are not final V1 release images because hardware qualification has not occurred.

## Findings and resolutions

### Finding A — stale SCO/eSCO requirements in the original SPEC

The original SPEC allowed SCO/eSCO to remain an optional V1 decision and referenced isochronous transport where supported. Implementation work made a definitive V1 decision instead:

- synchronous BR/EDR connections are configured to zero on the WROOM;
- the S3 exposes the legacy second Bluetooth interface only at alternate setting 0 with zero endpoints;
- no nonzero-bandwidth SCO alternate settings or usable isochronous endpoints are exposed; and
- HFP/HSP voice transport is out of V1 scope.

Resolution: consolidate that decision into the main SPEC and keep `V1_USB_COMPLIANCE_CORRECTION.md` as history.

### Finding B — stale development-board status

`LIMITATIONS.md` and `HARDWARE.md` retained wording from before V1-102 was closed.

Resolution: record the selected AYWHP ESP32-S3-DevKitC-1-N16R8 (ASIN `B0DG8L5NG5`) and Aideepen 30-pin ESP-WROOM-32 (ASIN `B0BQJ8BTVB`) as identified/approved; leave only physical electrical/UART validation open.

### Finding C — stale release-review caveat

`V1_SECURITY_REVIEW.md` still listed release log content as entirely unreviewed even after V1-1104 passed.

Resolution: distinguish completed static log-content/release-policy review from the still-open V1-904 physical timing/load audit.

### Finding D — historical correction file was carrying normative burden

The two-interface USB correction was implemented and reflected in the TODO/USB documentation, but the original SPEC still contained older wording.

Resolution: the main SPEC is reconciled so a reader no longer needs a sidecar correction to understand current V1 requirements.

## Development-only workaround audit

No checked V1 software item depends on substituting a development-only path for the intended product architecture:

- `firmware/bringup/esp32_wroom_uart_smoke/` and `firmware/bringup/esp32s3_uart_smoke/` are diagnostics only.
- Production CMake projects import `radio_h4` explicitly and do not import `radio_uart_smoke`.
- `scripts/check-component-boundaries.sh` enforces that separation.
- Normal host behavior is specified as USB Bluetooth HCI; no `btattach`, COM-port bridge, project daemon, project INF, or project Bluetooth driver is an accepted normal-operation dependency.
- Development serial logs do not carry normal host Bluetooth data.
- Development VID/PID `0xCAFE:0x4011` is explicitly not a production authorization.

Audit result: **no known open V1 blocker is hidden by a development-only workaround.**

## Explicitly open device evidence

The following remain open by design and are not software-audit failures:

- V1-103/V1-104 physical common-ground, UART, RTS/CTS, and reset-state validation;
- physical backpressure portion of V1-204;
- V1-306/V1-307 real controller/HCI/restart validation;
- V1-404 and physical portion of V1-405 native-USB enumeration/lifecycle validation;
- physical portion of V1-506 WROOM disappearance/reset behavior;
- all Linux G60 and Windows G70 acceptance;
- G80 recovery matrix;
- G90 UART-rate/performance/stability/logging-load measurements;
- G100 real peripheral compatibility matrix;
- physical validation portions of G120;
- hardware-qualified release artifacts and final Linux/Windows acceptance in V1-1301..1306.

## V1-1305 disposition

Software documentation/evidence audit: **PASS**.

The parent V1-1305 task remains open only because the specification/TODO must be re-audited against actual hardware-tested behavior after device testing resumes. That future pass must compare measured behavior against the documented contract rather than assuming the software-only audit proves physical interoperability.
