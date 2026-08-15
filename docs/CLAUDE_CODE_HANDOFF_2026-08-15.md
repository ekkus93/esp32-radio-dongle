# Claude Code Handoff — ESP32 Radio Dongle V1

Date: 2026-08-15

Repository: `ekkus93/esp32-radio-dongle`

Default branch: `master`

Baseline source commit immediately before this handoff document was added:

```text
bb51f9b86e66e2a67afc240b4e11ba6a0e5d8482
V1-104: guard UART smoke acceptance contract
```

Latest Firmware CI for that baseline:

```text
GitHub Actions run: 31877784132
Result: SUCCESS
Head: bb51f9b86e66e2a67afc240b4e11ba6a0e5d8482
```

This file is intended to let Claude Code take over the project without reconstructing the prior design decisions or repeating already-completed work.

---

## 1. Executive status

ESP32 Radio Dongle V1 is at the **hardware-qualification boundary**.

The substantive software, host-test, CI, release-configuration, architecture, and documentation work for V1 is complete. The remaining work is predominantly physical testing on the selected ESP32-S3 and ESP32-WROOM-32 development boards, followed by real Linux/Windows Bluetooth acceptance, recovery, performance, compatibility, and final release qualification.

No physical V1-103/V1-104/V1-204 acceptance run has been recorded yet in the repository. Do **not** infer a device PASS from the fact that the smoke firmware builds or that host CI is green.

The immediate intended sequence is:

1. physical V1-103 power/common-ground confirmation;
2. physical V1-104 bidirectional UART + RTS/CTS + reset smoke test;
3. close the remaining physical portion of V1-204 from the same measured backpressure test;
4. Ralph Loop V1-306 using the **production** WROOM controller and S3 bridge firmware;
5. continue through physical USB enumeration and full OS acceptance.

The canonical task tracker is:

```text
docs/ESP32_RADIO_DONGLE_V1_TODO.md
```

The canonical evidence index is:

```text
docs/V1_EVIDENCE_INDEX.md
```

The current specification is:

```text
docs/ESP32_RADIO_DONGLE_V1_SPEC.md
```

Treat those files as authoritative. This handoff summarizes them but does not replace them.

---

## 2. Product definition

V1 is a dual-MCU USB Bluetooth dongle:

```text
Windows / Linux host
        |
        | USB
        v
+-----------------------+
| ESP32-S3              |
| native USB device     |
| USB Bluetooth HCI     |
| USB <-> H4 bridge     |
+-----------+-----------+
            |
            | UART H4 + RTS/CTS
            v
+-----------------------+
| ESP32-WROOM-32        |
| Bluetooth controller  |
| BR/EDR + BLE          |
+-----------+-----------+
            |
            v
       2.4 GHz radio
```

The defining V1 requirement is:

> After both production firmware images are flashed and the boards are connected, plugging the ESP32-S3 **native USB** connection into a normal Windows or Linux computer must make the device behave as an ordinary USB Bluetooth Classic + BLE adapter without project-specific host software, custom Bluetooth drivers, helper daemons, or serial-HCI attach tools.

The host operating system owns the Bluetooth host stack and profiles.

### Explicit V1 non-goals

Do not expand V1 to include any of the following:

- Wi-Fi USB functionality;
- RTL8188EU emulation;
- custom Windows Bluetooth driver or INF;
- custom Linux Bluetooth driver;
- project host daemon/helper for ordinary operation;
- web configuration UI;
- Bluetooth profiles implemented on the S3;
- SCO/eSCO synchronous voice transport;
- HFP/HSP voice-audio acceptance.

Wi-Fi work is V2 only.

Bluetooth Classic itself **is** still required in V1. ACL-based Classic workloads such as HID and A2DP remain in scope.

---

## 3. Exact hardware selected for V1

V1-102 is complete. Do not reopen board identification unless the physical boards are actually substituted.

### ESP32-S3 board

Selected board:

```text
AYWHP ESP32-S3-DevKitC-1-N16R8 style development board
Amazon ASIN: B0DG8L5NG5
Purchase URL used by project owner:
https://www.amazon.com/dp/B0DG8L5NG5
```

Relevant properties already verified:

- ESP32-S3-WROOM-1 family;
- GPIO4, GPIO5, GPIO6, GPIO7 are available;
- native S3 USB device path is exposed;
- selected HCI pins are not S3 strapping pins;
- prior project USB-device enumeration exercised the S3 native USB path.

Prior project-owner Linux USB enumeration evidence for the S3 included:

```text
ID 303a:4001
ESP32 Macro Keyboard Project ESP32 Macro Keyboard
```

That prior identity is evidence that the physical native-USB device path has worked before. It is **not** the current radio-dongle VID/PID.

V1 radio-dongle development USB identity is:

```text
VID: 0xCAFE
PID: 0x4011
Manufacturer: ESP32 Radio Dongle
Product: ESP32 Radio Dongle V1
```

### ESP32-WROOM-32 board

Selected board:

```text
Aideepen 30-pin ESP-WROOM-32 development board
Amazon ASIN: B0BQJ8BTVB
Purchase URL used by project owner:
https://www.amazon.com/dp/B0BQJ8BTVB
```

Relevant properties already verified:

- original ESP32 / ESP-WROOM-32, not WROVER;
- GPIO16, GPIO17, GPIO25, GPIO26 are available;
- CP210x USB-UART is used for development flashing/logging;
- selected HCI pins are not original-ESP32 strapping pins.

Prior project-owner Linux USB enumeration evidence:

```text
ID 10c4:ea60
Silicon Labs CP210x UART Bridge
```

Board evidence is recorded in:

```text
docs/V1_BOARD_VERIFICATION.md
```

---

## 4. Locked inter-MCU electrical/HCI contract

Reference interconnect:

| ESP32-S3 | Direction | ESP32-WROOM-32 |
| --- | --- | --- |
| GPIO4 TX | -> | GPIO16 RX2 |
| GPIO5 RX | <- | GPIO17 TX2 |
| GPIO6 RTS | -> | GPIO25 CTS |
| GPIO7 CTS | <- | GPIO26 RTS |
| GND | <-> | GND |

ASCII form:

```text
ESP32-S3                                ESP32-WROOM-32
---------                               ----------------
GPIO4   TX  --------------------------> GPIO16  RX2
GPIO5   RX  <-------------------------- GPIO17  TX2
GPIO6   RTS --------------------------> GPIO25  CTS
GPIO7   CTS <-------------------------- GPIO26  RTS
GND         --------------------------- GND
```

### Electrical rules

These are already documented and statically audited:

- nominal GPIO logic is 3.3 V on both selected boards;
- no level shifter is required for the intended normally-powered direct connection;
- do not tie the two development boards' independently regulated 3.3 V rails together;
- do not tie 5 V/VBUS rails together for the two-board bring-up;
- connect common GND;
- each board should use one intended normal power source;
- do not insert a 5 V UART adapter into the HCI link;
- do not deliberately leave one board fully unpowered while the other actively drives the four direct GPIO signals;
- individual reset/EN tests are valid while VDD remains present on both boards;
- common cold-power testing should power both boards down/up together.

The electrical preflight is already PASS in:

```text
docs/V1_ELECTRICAL_PREFLIGHT.md
```

A CI policy check enforces the documented pin/direction/RTS-CTS source contract:

```text
scripts/check-electrical-contract.sh
```

---

## 5. Toolchain baseline

Both targets are pinned to:

```text
ESP-IDF v5.5.5 exactly
```

Do not silently build V1 against another ESP-IDF release.

Install/example activation is documented in:

```text
docs/BUILDING.md
```

Typical source installation:

```bash
git clone --branch v5.5.5 --recursive https://github.com/espressif/esp-idf.git ~/esp/esp-idf-v5.5.5
cd ~/esp/esp-idf-v5.5.5
./install.sh esp32,esp32s3
. ./export.sh
idf.py --version
```

Both firmware projects have target/version guards.

Project-owned C/C++ warning policy is effectively:

```text
-Wall -Wextra -Werror
```

C/C++ formatting uses repository `.clang-format`.

---

## 6. Repository firmware layout

Important paths:

```text
firmware/
  components/
    radio_h4/
    radio_uart_smoke/
  bringup/
    esp32_wroom_uart_smoke/
    esp32s3_uart_smoke/
  esp32_wroom_bt_controller/
  esp32s3_usb_bridge/

tests/
  host/

scripts/
  check-release-logging.sh
  check-component-boundaries.sh
  check-electrical-contract.sh
  check-doc-contract.sh

.github/workflows/
  firmware-ci.yml
  docs-ci.yml
```

Production builds intentionally do **not** depend on `radio_uart_smoke`. The smoke component is development-only and explicitly isolated from production component discovery.

---

## 7. Shared H4 implementation state

Shared HCI UART transport lives in:

```text
firmware/components/radio_h4/
```

Current baseline:

```text
RADIO_HCI_UART_BAUD = 115200
```

Supported parser/model packet types:

- HCI Command;
- HCI Event;
- HCI ACL;
- HCI SCO model packet.

The parser recognizes SCO framing for validation purposes, but the V1 production configuration rejects unexpected SCO/eSCO traffic because synchronous voice is explicitly outside V1 scope.

Implemented characteristics:

- bounded packet sizes;
- fixed-capacity queues;
- no unbounded steady-state allocation;
- independent direction-specific queues;
- queue high-water and queue-full counters;
- fragmented input handling;
- back-to-back packet handling;
- fail-closed malformed/oversized/truncated/trailing-data behavior;
- no silent mid-frame resynchronization that could forward ambiguous data.

Host tests:

```text
tests/host/test_radio_h4.c
```

V1-G20 is already PASS from host/software evidence.

### V1-204 status

V1-204 is only partially open:

Completed:

- S3 hardware RTS/CTS enabled;
- WROOM hardware RTS/CTS enabled.

Still physically required:

- prove backpressure occurs before receiver exhaustion;
- prove forwarding resumes cleanly when physical pressure clears.

The current V1-104 smoke test deliberately measures exactly these two behaviors, so a successful physical V1-104 run should also close the remaining V1-204 bullets.

---

## 8. ESP32-WROOM-32 production firmware state

Production project:

```text
firmware/esp32_wroom_bt_controller/
```

Main bridge source:

```text
firmware/esp32_wroom_bt_controller/main/wroom_bridge.c
```

Implemented:

- original ESP32 target;
- ESP-IDF v5.5.5;
- BR/EDR + BLE dual-mode controller initialization;
- controller-only/VHCI mode;
- no application-level Bluetooth profiles;
- UART2 H4 link;
- TX GPIO17;
- RX GPIO16;
- RTS GPIO26;
- CTS GPIO25;
- hardware RTS/CTS;
- UART0/CP210x retained for development logging/flashing;
- H4 validation in both directions;
- bounded queues;
- queue diagnostics;
- deterministic fail-closed restart/recovery behavior;
- synchronous connections disabled for V1.

The production WROOM logs the following important state when successful:

```text
Bluetooth controller enabled in BR/EDR + BLE mode
UART2 H4: baud=115200 TX=17 RX=16 RTS=26 CTS=25
```

V1-301 through V1-305 are complete.

V1-306 and V1-307 remain physically open.

---

## 9. ESP32-S3 production firmware state

Production project:

```text
firmware/esp32s3_usb_bridge/
```

Main bridge source:

```text
firmware/esp32s3_usb_bridge/main/s3_bridge.c
```

Implemented UART mapping:

```text
UART1
TX  = GPIO4
RX  = GPIO5
RTS = GPIO6
CTS = GPIO7
baud = 115200
hardware flow control = CTS_RTS
```

The production S3 performs a real controller-readiness probe **before installing/attaching normal USB Bluetooth service**.

Current startup sequence:

1. initialize H4 UART;
2. enter `WAIT_CONTROLLER`;
3. send HCI Reset (`opcode 0x0C03`);
4. require matching successful Command Complete;
5. send HCI Read Local Version Information (`opcode 0x1001`);
6. require matching successful Command Complete/version payload;
7. log actual returned controller version/manufacturer information;
8. enter `CONTROLLER_READY`;
9. start bridge tasks;
10. install/attach USB Bluetooth service.

Expected success log from the production S3 includes:

```text
WROOM controller probe passed: hci_version=0x.. hci_revision=0x.... lmp_version=0x.. manufacturer=0x.... lmp_subversion=0x....
```

This existing probe is highly relevant to V1-306. Do not create a second incompatible HCI path merely to test the same transport.

If the WROOM probe fails, the S3 intentionally does not fabricate successful HCI responses and does not pretend a usable controller exists.

---

## 10. USB Bluetooth implementation state

The S3 uses the normal ESP32-S3 native USB device peripheral.

The project does **not** rely on a generic USB-UART link for final Bluetooth operation.

### USB identity

```text
Device/interface class:    0xE0 Wireless Controller
Subclass:                  0x01 Bluetooth
Protocol:                  0x01 Primary Controller
Development VID:           0xCAFE
Development PID:           0x4011
Manufacturer:              ESP32 Radio Dongle
Product:                   ESP32 Radio Dongle V1
Serial:                    stable factory-MAC-derived value
```

The development VID/PID is not authorization for production distribution. Final distributed firmware must use a VID/PID the project is authorized to use.

### Locked two-interface V1 layout

The earlier single-interface planning language was corrected. The current SPEC itself now carries the correct two-interface contract.

Interface 0, alt 0:

- E0/01/01;
- HCI Command via endpoint-0 class control transfer;
- Event Interrupt IN endpoint `0x81`, max packet 16;
- ACL Bulk OUT endpoint `0x02`, max packet 64;
- ACL Bulk IN endpoint `0x82`, max packet 64.

Interface 1, alt 0:

- E0/01/01;
- zero endpoints;
- zero active voice bandwidth.

Configuration descriptor:

```text
bNumInterfaces = 2
total configuration length = 48 bytes
```

There are no nonzero-bandwidth SCO alternate settings and no usable isochronous voice endpoints.

### Project-owned USB class shim

The project uses:

```text
radio_usb_bth
```

rather than enabling the pinned stock TinyUSB BTH class as the production transport.

Host tests compile and exercise the actual production USB class and descriptor source against a minimal fake TinyUSB backend.

Relevant tests:

```text
tests/host/test_radio_usb_bth.c
tests/host/test_radio_usb_bth_control_compat.c
tests/host/test_usb_descriptors.c
```

Covered behavior includes:

- descriptor/open validation;
- strict primary interface handling;
- endpoint-free interface 1 claim behavior;
- HCI class control transfers;
- legacy device-targeted request compatibility including historical `bRequest=0xE0`;
- rejection of wrong-direction/misrouted/non-class/oversized requests;
- fragmented ACL OUT reassembly;
- incomplete/oversized/ambiguous ACL rejection;
- event validation;
- ACL IN ZLP termination when required;
- transfer-error/reset behavior;
- exact production descriptor bytes;
- stable serial formatting and fallback behavior.

V1-401 through V1-403 are complete. Physical V1-404 and the physical portion of V1-405 remain open.

Historical design-decision record:

```text
docs/V1_USB_COMPLIANCE_CORRECTION.md
```

Current normative documents:

```text
docs/ESP32_RADIO_DONGLE_V1_SPEC.md
docs/USB_BLUETOOTH_V1.md
```

---

## 11. Security/release state

Software-side release/security review is complete.

Relevant files:

```text
docs/V1_SECURITY_REVIEW.md
docs/V1_RELEASE_CONFIGURATION.md
firmware/esp32_wroom_bt_controller/sdkconfig.release
firmware/esp32s3_usb_bridge/sdkconfig.release
scripts/check-release-logging.sh
scripts/check-component-boundaries.sh
```

Release profiles use WARN-only default/maximum application logging and reduced bootloader logging. Production trees are checked against raw buffer/hex dump logging APIs.

Bring-up smoke firmware is development-only and must not leak into production component discovery.

V1-G110 is PASS for software configuration.

V1-904 remains physically open because only sustained real traffic can prove that logging/timing behavior is acceptable on hardware.

---

## 12. CI state

Latest firmware baseline before this handoff document:

```text
commit: bb51f9b86e66e2a67afc240b4e11ba6a0e5d8482
Firmware CI run: 31877784132
result: SUCCESS
```

That run validates the current V1-104 smoke-test acceptance guard as well as the normal firmware CI matrix.

A prior fully indexed software/release checkpoint is:

```text
commit: 57864e9e83b5760c70fbff2e3b5f1ab1cfbe174e
Firmware CI run: 31873127842
result: SUCCESS
```

That indexed run passed:

- C formatting;
- release-logging policy;
- component-boundary policy;
- H4 host tests;
- USB class host tests;
- USB control compatibility tests;
- descriptor host tests;
- normal WROOM production build;
- normal S3 production build;
- both UART smoke builds;
- WROOM release build;
- S3 release build.

Development release artifact archive digests recorded for that run:

```text
WROOM artifact:
v1-release-esp32-controller-57864e9e83b5760c70fbff2e3b5f1ab1cfbe174e
sha256:a4d9e6559a3f42886395e3430e4b9574b6e2d81e38165c6ba01dfffa253e9b88

S3 artifact:
v1-release-esp32s3-bridge-57864e9e83b5760c70fbff2e3b5f1ab1cfbe174e
sha256:ef88512964d3bac8de6e2c60d37912cf1b062cd80118cc4564c0aae41aa40cae
```

These are development-pipeline evidence only. They are **not** final V1 release artifacts because hardware qualification has not happened.

The exact final release artifacts must be rebuilt/captured from the eventual hardware-qualified release commit.

Documentation contract CI also exists:

```text
.github/workflows/docs-ci.yml
scripts/check-doc-contract.sh
```

The V1-1305 software documentation/evidence audit is PASS. Its parent remains open solely for the post-device-test re-audit against actual measured behavior.

---

# 13. IMMEDIATE TASK — physical V1-103 / V1-104 / V1-204

This is the first task Claude Code should perform if it has access to the physical boards.

Do not jump directly to host USB/BlueZ testing until the inter-MCU link has passed this bring-up.

Canonical procedure:

```text
docs/V1_UART_BRINGUP.md
```

Evidence record to fill in:

```text
docs/V1_UART_BRINGUP_EVIDENCE.md
```

## 13.1 Detect the development ports

Do not assume `/dev/ttyUSB0` or `/dev/ttyACM0` without checking.

Useful Linux commands:

```bash
lsusb
ls -l /dev/serial/by-id/ 2>/dev/null || true
ls -l /dev/ttyUSB* /dev/ttyACM* 2>/dev/null || true
dmesg --follow
```

The WROOM board has previously appeared as CP210x USB ID `10c4:ea60`.

Record the actual serial paths used in the evidence file if practical.

## 13.2 Wire with both boards unpowered

Connect exactly:

```text
S3 GPIO4  -> WROOM GPIO16
S3 GPIO5  <- WROOM GPIO17
S3 GPIO6  -> WROOM GPIO25
S3 GPIO7  <- WROOM GPIO26
S3 GND    <-> WROOM GND
```

Do not connect the two 3.3 V rails.

Do not connect the two 5 V/VBUS rails.

Use a safe/common power method that keeps both MCU I/O domains normally powered while the four direct GPIOs are active.

## 13.3 Flash WROOM smoke firmware

With ESP-IDF v5.5.5 active:

```bash
cd firmware/bringup/esp32_wroom_uart_smoke
idf.py fullclean
idf.py set-target esp32
idf.py build
idf.py -p <WROOM_SERIAL_PORT> flash monitor
```

Expected important output:

```text
V1-103/V1-104 ESP32-WROOM UART/RTS/CTS bring-up image
WROOM responder: UART=2 baud=115200 TX=17 RX=16 RTS=26 CTS=25 flow=CTS_RTS threshold=96
READY: waiting for ESP32-S3 smoke-test initiator
```

The smoke firmware reads back the ESP-IDF hardware-flow-control mode and refuses to continue if it is not `UART_HW_FLOWCTRL_CTS_RTS`.

## 13.4 Flash S3 smoke firmware

In a second terminal:

```bash
cd firmware/bringup/esp32s3_uart_smoke
idf.py fullclean
idf.py set-target esp32s3
idf.py build
idf.py -p <S3_DEBUG_SERIAL_PORT> flash monitor
```

For this smoke test, use the development flashing/debug connection, not the final Bluetooth native-USB host path.

Expected configuration line:

```text
S3 initiator: UART=1 baud=115200 TX=4 RX=5 RTS=6 CTS=7 flow=CTS_RTS threshold=96
```

## 13.5 What a full automated round proves

### Phase A — basic bidirectional UART

S3 transmits 32 sequenced frames and WROOM echoes each byte-for-byte.

Required output:

```text
PASS: 32 bidirectional ping/echo frames
```

This is the main V1-104 evidence for:

- S3 -> WROOM TX/RX;
- WROOM -> S3 TX/RX.

### Phase B — WROOM RTS -> S3 CTS

WROOM pauses its UART RX interrupt for 600 ms while S3 attempts to transmit a deterministic 1024-byte payload.

Required:

- S3 transmit drain time >= 350 ms;
- all 1024 bytes arrive intact after receiver resumes;
- no corruption;
- subsequent communication resumes normally.

Expected output:

```text
PASS: WROOM RTS -> S3 CTS backpressure asserted/released; 1024-byte payload drained in <N> ms
```

### Phase C — S3 RTS -> WROOM CTS

S3 pauses its UART RX interrupt for 600 ms while WROOM transmits the deterministic 1024-byte payload.

Required:

- WROOM measured drain time >= 350 ms;
- all 1024 bytes arrive intact;
- communication resumes after RTS releases.

Expected output:

```text
PASS: S3 RTS -> WROOM CTS backpressure asserted/released; 1024-byte payload drained in <N> ms
```

### Complete round

Required output:

```text
ROUND <N> PASS: TX/RX and both RTS/CTS crossings asserted, throttled, released, and resumed
BRINGUP PASS: round=<N>
```

After round 1:

```text
RESET TEST READY: keep both boards powered; reset either MCU and require a later round PASS
```

The smoke harness intentionally repeats complete rounds rather than exiting after one PASS.

## 13.6 Reset/re-synchronization qualification

After at least one initial complete round PASS, keep all five wires attached and keep both boards powered.

### WROOM reset while S3 stays powered

Perform 5 cycles:

1. note current round number;
2. press WROOM EN/reset;
3. WROOM must boot normally;
4. S3 must automatically re-synchronize;
5. require a later **complete** round PASS.

A boot banner without a later full round does not count.

### S3 reset while WROOM stays powered

Perform 5 cycles:

1. leave WROOM powered;
2. press S3 EN/reset;
3. S3 must boot normally and restore `flow=CTS_RTS`;
4. S3 must re-synchronize with still-running WROOM;
5. require a complete post-reset round PASS.

### Common cold-power cycles

Perform 5 cycles with both boards powered down/up together using the same safe common-power arrangement.

Each cycle must reach a complete round PASS.

Do not test one board fully powered off while the peer continues driving direct GPIO unless isolation/sequencing hardware is separately designed and qualified. That condition is outside the current raw-GPIO electrical contract.

## 13.7 Evidence updates after PASS

Update:

```text
docs/V1_UART_BRINGUP_EVIDENCE.md
docs/ESP32_RADIO_DONGLE_V1_TODO.md
docs/V1_EVIDENCE_INDEX.md
```

Record at minimum:

- test date;
- actual board/port references;
- confirmation common GND is connected;
- confirmation no power rails were tied together;
- power-up method;
- wiring confirmation/photo reference if available;
- WROOM UART config line;
- S3 UART config line;
- Phase A log;
- Phase B measured drain time;
- Phase C measured drain time;
- payload integrity results;
- five WROOM reset results;
- five S3 reset results;
- five common cold-power results;
- no boot-strap conflict observed.

If all acceptance criteria pass, the physical evidence should allow:

- V1-103 -> PASS;
- V1-104 -> PASS;
- V1-G10 -> PASS;
- remaining two physical V1-204 bullets -> PASS;
- V1-204 parent -> PASS.

Do not mark any of those if the hardware evidence is incomplete.

---

# 14. NEXT TASK — Ralph Loop V1-306

After V1-103/V1-104/V1-204 pass, restore the **production** firmware and test raw controller operation.

V1-306 canonical subtasks are:

- send a basic HCI command over the physical inter-MCU link;
- receive the corresponding HCI event;
- read controller identity/version information from hardware;
- confirm BR/EDR capability on the actual controller;
- confirm BLE capability on the actual controller.

## 14.1 Restore production WROOM firmware

```bash
cd firmware/esp32_wroom_bt_controller
idf.py fullclean
idf.py -DIDF_TARGET=esp32 build
idf.py -p <WROOM_SERIAL_PORT> flash monitor
```

Expected key log:

```text
Bluetooth controller enabled in BR/EDR + BLE mode
UART2 H4: baud=115200 TX=17 RX=16 RTS=26 CTS=25
```

## 14.2 Restore production S3 firmware

In another terminal:

```bash
cd firmware/esp32s3_usb_bridge
idf.py fullclean
idf.py -DIDF_TARGET=esp32s3 build
idf.py -p <S3_PROGRAMMING_PORT> flash monitor
```

Keep the inter-MCU H4 wiring installed.

The production S3 already sends real HCI commands during startup:

```text
HCI Reset                         opcode 0x0C03
HCI Read Local Version Information opcode 0x1001
```

The S3 requires successful Command Complete responses before attaching normal USB Bluetooth service.

A successful hardware path should produce a log like:

```text
WROOM controller probe passed: hci_version=0x.. hci_revision=0x.... lmp_version=0x.. manufacturer=0x.... lmp_subversion=0x....
```

This directly supplies physical evidence for the first three V1-306 bullets if the logs are captured from the connected hardware.

## 14.3 Capability confirmation caveat

Do **not** mark the final two V1-306 capability bullets merely because the WROOM firmware is configured with `ESP_BT_MODE_BTDM` or because CI compiled it.

Obtain actual hardware evidence for both BR/EDR and BLE controller capability.

Preferred approach: extend the existing production startup diagnostic/probe in a standards-correct, bounded way if necessary, or use a dedicated hardware diagnostic that reuses the same H4 transport. Suitable standard HCI capability commands may include controller supported-features/supported-commands queries and LE-specific capability queries; decode them according to the Bluetooth Core specification rather than guessing feature bits.

Do not create a parallel UART protocol, mock response path, or fabricated capability result.

If later Linux USB enumeration already exposes reliable controller capability information through the standard host stack, that can be cross-evidence, but V1-306 should still clearly demonstrate the real WROOM controller across the physical H4 link.

## 14.4 Evidence for V1-306

Create or extend a focused evidence record rather than burying the result in chat/terminal history.

Recommended evidence:

- production firmware commit SHA;
- exact WROOM/S3 board identities;
- UART rate/pin logs;
- HCI Reset command/Command Complete evidence;
- Read Local Version command/result;
- returned HCI/LMP version, manufacturer, revisions/subversion;
- standards-based BR/EDR capability evidence;
- standards-based LE capability evidence;
- any diagnostics/counters relevant to malformed packets or recovery;
- explicit PASS/FAIL disposition.

Then update the canonical TODO and evidence index.

---

# 15. V1-307 — production reset/restart scenarios

After V1-306, test the production bridge/controller reset scenarios; the smoke-test reset results are valuable electrical evidence but should not substitute for production-firmware recovery behavior.

Required V1-307:

- WROOM reboot while S3 remains powered;
- S3 reboot while WROOM remains powered;
- repeated WROOM reset/power cycles.

Important distinction:

- reset with both boards' VDD still present is in the current electrical contract;
- removing power entirely from one raw-GPIO peer while the other remains active is not yet qualified and must not be assumed safe/required.

V1-G30 closes only after V1-306 and V1-307 have real hardware evidence.

---

# 16. V1-404 / V1-405 — physical USB enumeration/lifecycle

Once the WROOM production controller path is proven, move to the S3 native USB Bluetooth device.

### V1-404 requirements

On a physical Linux host:

- capture the full device/configuration/interface/endpoint descriptors;
- verify E0/01/01 identity on the wire;
- verify configuration total length 48;
- verify two interfaces;
- verify interface 0 endpoints:
  - `0x81` Interrupt IN, max packet 16;
  - `0x02` Bulk OUT, max packet 64;
  - `0x82` Bulk IN, max packet 64;
- verify interface 1 alt 0 has zero endpoints;
- verify development VID/PID `CAFE:4011`;
- verify stable serial behavior;
- verify repeated physical enumeration is stable.

Useful Linux tools may include:

```bash
lsusb
lsusb -v -d cafe:4011
dmesg --follow
usb-devices
```

Do not mark V1-404 from host-unit descriptor tests alone; those are already green and are only software evidence.

### V1-405 remaining physical requirement

Source lifecycle states are implemented. Physical testing must still verify clean behavior after real:

- USB reset;
- suspend/resume;
- unplug/replug;
- disconnect/recovery.

V1-G40 remains open until these physical checks pass.

---

# 17. V1-506 / V1-G50 — full bridge integration

Software command/event/ACL paths already exist.

V1-506 completed software items:

- real startup HCI Reset + Read Local Version probe;
- no fabricated controller success;
- fatal transport/USB protocol corruption causes controlled recovery rather than forwarding garbage.

Remaining V1-506 item:

- physically verify behavior when the WROOM disappears/resets **after USB enumeration**.

V1-G50 requires a real end-to-end host transaction:

```text
USB host
 -> S3 USB HCI
 -> S3 H4 UART
 -> WROOM controller
 -> WROOM H4 UART
 -> S3
 -> USB host
```

Do not close G50 from the startup probe alone; the startup probe is pre-USB and does not establish a host-originated end-to-end round trip.

---

# 18. Linux driverless acceptance — V1-G60

Open tasks:

### V1-601 generic binding

- plug S3 native USB into Linux;
- normal kernel USB Bluetooth driver binds;
- HCI controller appears;
- BlueZ sees controller;
- no `btattach`;
- no project kernel module;
- no project daemon.

Useful diagnostics:

```bash
lsusb
lsusb -t
dmesg --follow
rfkill list
bluetoothctl list
bluetoothctl show
btmgmt info
```

Use actual host tools available on the test system; do not install a project-specific helper just to make acceptance pass.

### V1-602 BLE basics

- scan;
- connect;
- representative GATT operation;
- disconnect;
- reconnect/repeated operation.

### V1-603 Classic basics

- inquiry/discovery;
- pair;
- connect/disconnect;
- reconnect after replug.

### V1-604 Classic/Bluetooth HID

Pair a real keyboard or mouse and confirm real input and reconnect behavior.

### V1-605 high ACL load

Use A2DP or another sustained ACL workload. Inspect bridge diagnostics for:

- H4 corruption;
- queue-full counters;
- UART errors;
- uncontrolled queue growth;
- unexpected recovery.

G60 remains open until actual Linux acceptance passes.

---

# 19. Windows driverless acceptance — V1-G70

Open tasks:

### V1-701 automatic binding

- cold-plug into supported Windows;
- Windows identifies it as Bluetooth using the in-box stack;
- no project INF/custom driver installed;
- normal Windows Bluetooth UI appears.

### V1-702 BLE basics

- scan;
- pair/connect;
- representative BLE function;
- disconnect/reconnect.

### V1-703 Classic basics

- discover;
- pair;
- connect/disconnect;
- reconnect after replug.

### V1-704 HID

Pair keyboard/mouse; verify actual input plus reboot/replug reconnect.

### V1-705 high ACL load

Use A2DP or another sustained ACL workload and inspect bridge diagnostics.

G70 remains open until actual Windows acceptance passes.

---

# 20. Recovery/robustness — V1-G80

Open physical matrix:

### V1-801 USB unplug/replug

- 10 cycles Linux;
- 10 cycles Windows;
- no reflashing/manual recovery.

### V1-802 host reboot

- Linux boot with dongle attached;
- Windows boot with dongle attached;
- Bluetooth becomes available normally.

### V1-803 WROOM reset recovery

- reset WROOM while S3 powered;
- no stale/corrupt forwarding;
- controller service returns predictably.

### V1-804 S3 reset recovery

- reset S3 while WROOM powered;
- USB re-enumerates;
- HCI re-synchronizes.

### V1-805 UART corruption/error behavior

Already software-covered:

- malformed/truncated/oversized H4 host tests;
- parser fail-closed behavior.

Still physical where practical:

- hardware UART receive overflow/error path.

### V1-806 queue pressure

Already software-covered:

- fixed queue exhaustion detection/counting.

Still physical:

- high host-to-controller pressure;
- high controller-to-host pressure;
- physical RTS/CTS prevents corruption;
- forwarding resumes afterward.

### V1-807 suspend/resume

- Linux suspend/resume;
- Windows sleep/resume where supported;
- no reflashing required.

---

# 21. Performance/stability — V1-G90

### V1-901 final UART baud

Current baseline is intentionally conservative:

```text
115200 baud
```

Remaining:

- measure baseline on hardware;
- test progressively higher rates;
- record error/high-water behavior;
- select the fastest comfortably stable rate;
- update shared configuration only after evidence supports the change.

Do not increase baud merely because 115200 seems slow.

### V1-902 sustained traffic

At least 30 minutes representative ACL traffic:

- zero bridge-caused malformed H4 packets;
- no uncontrolled queue growth;
- record throughput;
- record queue high-water marks.

### V1-903 multi-hour stability

- multi-hour active Linux run;
- multi-hour active Windows run;
- record resets/USB errors/UART errors/disconnects.

### V1-904 logging load audit

Static/release configuration is already complete.

Still required:

- verify development logging cannot starve USB/UART during real hardware traffic;
- reduce/rate-limit timing-sensitive logs only if physical evidence shows it is needed.

---

# 22. Bluetooth compatibility — V1-G100

### V1-1001 BLE peripheral matrix

Use at least a simple BLE GATT peripheral and exercise repeated scanning/connection/pairing/bonding where supported.

### V1-1002 Classic HID matrix

Use real Classic HID keyboard/mouse where available. Record actual peripheral models.

### V1-1003 Classic audio/ACL stress

Use an A2DP endpoint where available. Record stability and throughput limitations.

### V1-1004 SCO/eSCO decision

Already complete.

Do **not** reopen SCO/eSCO as a V1 implementation task.

V1 intentionally advertises zero active voice bandwidth and provides no usable synchronous USB transport.

### V1-1005 cross-host regression

Re-test selected peripherals on both Linux and Windows and distinguish host-stack-specific issues from firmware defects.

---

# 23. Documentation/finalization tasks that remain open only because hardware evidence does not exist yet

## V1-1206 Known limitations

Already records the no-SCO decision and selected-board status.

Still must eventually record actual:

- tested Linux OS versions/builds;
- tested Windows versions/builds;
- tested Bluetooth peripherals;
- measured throughput/stability observations.

## V1-1301 final build capture

Clean CI builds and development artifact hashing already work.

Remaining:

- once hardware qualification is complete, rebuild/capture artifacts from the **exact qualified release commit**;
- record exact names, hashes, and flash inputs.

Do not promote old development artifacts to final release artifacts.

## V1-1302 Linux final clean acceptance

Requires full clean Linux run without project host software.

## V1-1303 Windows final clean acceptance

Requires full clean Windows run without project host software.

## V1-1305 final hardware-behavior re-audit

Software/documentation audit is already PASS.

After physical qualification, re-audit:

```text
docs/ESP32_RADIO_DONGLE_V1_SPEC.md
docs/ESP32_RADIO_DONGLE_V1_TODO.md
docs/LIMITATIONS.md
docs/V1_EVIDENCE_INDEX.md
```

against what the hardware actually did.

Do not change requirements retroactively just to make a failing test look complete. If a deliberate spec change is necessary, document the engineering reason explicitly.

## V1-1306 declare V1 complete

Only after:

- all mandatory tasks are complete or deliberately superseded by documented spec change;
- Linux acceptance passes;
- Windows acceptance passes;
- release notes exist;
- final hardware-qualified release artifacts are captured.

---

# 24. Current canonical task state summary

## Complete

### Repository/toolchain

- V1-001
- V1-002
- V1-003
- V1-004
- V1-G00 PASS

### Hardware definition

- V1-101
- V1-102
- V1-103 static/datasheet subitems except physical common ground

### H4 transport

- V1-201
- V1-202
- V1-203
- V1-205
- V1-206
- V1-G20 PASS

### WROOM software

- V1-301
- V1-302
- V1-303
- V1-304
- V1-305

### S3 USB software

- V1-401
- V1-402
- V1-403
- software state-machine portion of V1-405

### Bridge software

- V1-501
- V1-502
- V1-503
- V1-504
- V1-505
- software portion of V1-506
- V1-507

### Security/release

- V1-1101
- V1-1102
- V1-1103
- V1-1104
- V1-G110 software configuration PASS

### Documentation

- V1-1201
- V1-1202
- V1-1203
- V1-1204
- V1-1205
- software/documented parts of V1-1206

### Final software checks

- V1-1304
- software portion of V1-1305

## Partially complete / waiting on physical evidence

- V1-103
- V1-104
- V1-204
- V1-306
- V1-307
- V1-404
- V1-405
- V1-506
- V1-805
- V1-806
- V1-901
- V1-904
- V1-1206
- V1-1301
- V1-1305

## Entirely or essentially hardware/host acceptance

- V1-G10 remaining physical bring-up
- V1-G30 controller hardware acceptance
- V1-G40 USB hardware acceptance
- V1-G50 integrated bridge hardware acceptance
- V1-G60 Linux
- V1-G70 Windows
- V1-G80 recovery/robustness
- V1-G90 performance/stability
- V1-G100 compatibility
- V1-G120 final physical documentation walk-through
- V1-1302
- V1-1303
- V1-1306
- V1-FINAL

---

# 25. Important documentation map

Claude Code should read these before making architectural changes:

```text
docs/ESP32_RADIO_DONGLE_V1_SPEC.md
    Current V1 normative architecture/acceptance contract.

docs/ESP32_RADIO_DONGLE_V1_TODO.md
    Canonical task tracker and gates.

docs/V1_EVIDENCE_INDEX.md
    What is actually proven by software versus physical evidence.

docs/V1_BOARD_VERIFICATION.md
    Exact development-board selection and compatibility evidence.

docs/V1_ELECTRICAL_PREFLIGHT.md
    Datasheet-backed voltage/pin/strap/power assumptions.

docs/HARDWARE.md
    User-facing wiring and power contract.

docs/V1_UART_BRINGUP.md
    Immediate physical V1-103/V1-104/V1-204 procedure.

docs/V1_UART_BRINGUP_EVIDENCE.md
    Evidence template to fill during the immediate bench test.

docs/USB_BLUETOOTH_V1.md
    Current USB Bluetooth transport contract.

docs/V1_USB_COMPLIANCE_CORRECTION.md
    Historical record of the two-interface correction.

docs/V1_SECURITY_REVIEW.md
    Security/failure-semantics review.

docs/V1_RELEASE_CONFIGURATION.md
    Release logging/configuration contract.

docs/V1_DOCUMENTATION_EVIDENCE_AUDIT.md
    V1-1305 software documentation/evidence audit.

docs/BUILDING.md
    Toolchain/build instructions.

docs/FLASHING.md
    Production flashing instructions.

docs/USAGE.md
    Intended normal host behavior.

docs/TROUBLESHOOTING.md
    Failure diagnosis.

docs/LIMITATIONS.md
    Current documented product limitations.
```

---

# 26. Important implementation/test map

```text
firmware/components/radio_h4/
    Shared bounded H4 parser/model/queues.

firmware/components/radio_uart_smoke/
    Shared V1-103/V1-104/V1-204 physical smoke logic.

firmware/bringup/esp32_wroom_uart_smoke/
    Dedicated WROOM responder image.

firmware/bringup/esp32s3_uart_smoke/
    Dedicated S3 initiator image.

firmware/esp32_wroom_bt_controller/
    Production WROOM Bluetooth controller bridge.

firmware/esp32s3_usb_bridge/
    Production S3 native-USB Bluetooth bridge.

tests/host/test_radio_h4.c
    Shared H4 host tests.

tests/host/test_radio_usb_bth.c
    Production USB class host tests.

tests/host/test_radio_usb_bth_control_compat.c
    Legacy/strict control-request compatibility tests.

tests/host/test_usb_descriptors.c
    Byte-level production descriptor tests.

scripts/check-electrical-contract.sh
    Pin/direction/flow-control and V1-104 smoke acceptance guard.

scripts/check-component-boundaries.sh
    Prevents bring-up component leakage into production.

scripts/check-release-logging.sh
    Release logging/raw-dump policy.

scripts/check-doc-contract.sh
    Prevents stale V1 normative documentation from returning.
```

---

# 27. Recent important commits/history

These commits explain some non-obvious current state:

```text
bb51f9b86e66e2a67afc240b4e11ba6a0e5d8482
V1-104: guard UART smoke acceptance contract

c9f294675c183be16ea00b7917239a6ef1ce045a
Formatting fix for hardened V1-104 smoke source

d3fc0176f7c7949c84e9359a632fd681281a46a8
V1-104: harden UART reset and resync smoke test

ce37dddd0a4dea1722358699c76dfaa390e98ded
Docs: record V1-102 board evidence

d1978d58abdb72cd847e0c88a3538e93461c3f9f
Docs: mark selected V1 boards verified

f714651768606437c3efb31fb0701dd43bcd7723
Docs: close V1-102 board verification

b033e70c806d4e013157745e09a329f8affd8f19
Docs: synchronize V1 software checkpoint

7c0ae8aa0167d305f15e993632744f184614289d
USB compliance correction record

c6a9cef836ede3a21b1e7b70590dbcafdf07a170
USB documentation two-interface update

f55a4f9023c994fe00910b2f8c476b4d41a05a8e
Release CI quote-safe fix

cdff0e04819342011ddcdfe044fc8c50fe714af0
Target-scope UART smoke component constants/discovery

6de223499aae403d0a21f6addc047df37b8defb0
Removed unused production SCO runtime path
```

Do not infer that an old document or old planning comment predating these commits is still normative.

---

# 28. Engineering invariants Claude Code must preserve

1. **Do not mark physical acceptance PASS from CI or source inspection.**
2. **Do not reopen V1-102** unless the actual development boards are changed.
3. **Use ESP-IDF v5.5.5 exactly** for current V1 qualification.
4. Preserve the reference H4 UART mapping unless a deliberate hardware revision is documented.
5. Preserve hardware RTS/CTS; do not “fix” failures by disabling flow control.
6. Keep the H4 path bounded and fail-closed.
7. Never fabricate successful HCI controller responses.
8. Keep production builds free of development smoke component dependencies.
9. Keep debug/diagnostic bytes off the USB HCI endpoints.
10. Keep the current two-interface USB Bluetooth descriptor contract.
11. Do not reintroduce pseudo-SCO or nonzero voice-bandwidth alternate settings.
12. SCO/eSCO/HFP/HSP voice remains out of V1.
13. Wi-Fi remains V2.
14. Normal user operation must not depend on a project-specific host utility/daemon/driver.
15. Do not parallel the two boards' independent power rails during prototype testing.
16. Do not assume a powered GPIO may safely drive a completely unpowered peer.
17. Final artifacts must be produced from the final hardware-qualified commit, not an earlier green CI commit.
18. Update evidence and TODO state immediately when a physical acceptance criterion is actually proven.

---

# 29. Recommended Ralph Loop workflow for the remainder

For each open hardware task:

1. Read the exact canonical TODO bullets.
2. Read the corresponding procedure/evidence file.
3. Inspect current production/test code before changing anything.
4. Run the smallest physical experiment that answers the current acceptance question.
5. Preserve raw logs and measured values.
6. If it fails, diagnose the actual phase/signal rather than weakening acceptance criteria.
7. If code changes are needed:
   - make the smallest bounded fix;
   - run formatting/policy/host tests;
   - build both relevant ESP-IDF targets;
   - rerun the physical experiment.
8. When and only when evidence passes:
   - update the dedicated evidence document;
   - update `docs/V1_EVIDENCE_INDEX.md`;
   - check the exact TODO subtasks;
   - update the gate status.
9. Commit the logical unit of work with a descriptive commit message.
10. Continue to the next dependency-bound task.

Do not batch unrelated speculative refactors into hardware-debugging commits.

---

# 30. First action for Claude Code

Assuming Claude Code can access both physical development boards, the first action should be:

```text
Run the physical procedure in docs/V1_UART_BRINGUP.md and fill
`docs/V1_UART_BRINGUP_EVIDENCE.md`.
```

Do the first automated complete round before the reset matrix.

If the first round fails, classify it immediately:

- cannot synchronize: power/GND/TX-RX/RTS-CTS/pin/image/baud issue;
- ping fails: basic TX/RX path issue;
- ping passes, Phase B fails: focus WROOM GPIO26 RTS -> S3 GPIO7 CTS;
- Phase B passes, Phase C fails: focus S3 GPIO6 RTS -> WROOM GPIO25 CTS;
- payload corrupts: real flow-control failure;
- measured block <350 ms: RTS/CTS likely not actually throttling;
- reset boots but no later round passes: re-sync/state recovery failure.

Once V1-103/V1-104/V1-204 are physically green and recorded, immediately restore production firmware and Ralph Loop **V1-306 — Verify raw controller operation** using the existing production HCI Reset + Read Local Version readiness probe as the starting point.

---

# 31. Definition of handoff success

This handoff is successful if Claude Code can begin with the physical UART/RTS-CTS bring-up without asking the project owner to rediscover:

- which boards are being used;
- which pins are wired;
- which ESP-IDF version is required;
- why SCO is disabled;
- why USB uses two interfaces;
- what software is already complete;
- what CI already proves;
- what CI does **not** prove;
- what the exact next physical acceptance test is;
- how V1-204 is tied to the V1-104 backpressure test;
- how V1-306 should reuse the existing production HCI probe;
- which later Linux/Windows/recovery/performance/finalization tasks remain.

The project is not waiting for additional speculative software work. It is waiting for **measured physical and host acceptance evidence**.
