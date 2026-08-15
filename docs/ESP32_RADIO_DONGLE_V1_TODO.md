# ESP32 Radio Dongle V1 TODO

This TODO implements `docs/ESP32_RADIO_DONGLE_V1_SPEC.md`.

V1 is complete only when the assembled ESP32-S3 + ESP32-WROOM-32 device behaves as a normal USB Bluetooth Classic + BLE adapter on Windows and Linux with **no project-specific host-side software or custom Bluetooth driver required for normal operation**.

Wi-Fi is deferred to V2.

## Status legend

- `[ ]` not complete or requires additional evidence
- `[x]` complete with the required software/documentation evidence

Do not mark hardware or host acceptance complete from source code or CI alone.

## Current evidence checkpoint

Software/documentation checkpoint as of 2026-08-15:

- ESP-IDF is pinned to **v5.5.5** for both targets.
- GitHub Actions run **31873127842**, commit `57864e9e83b5760c70fbff2e3b5f1ab1cfbe174e`, passed:
  - C formatting validation;
  - release-logging and component-boundary policy checks;
  - strict host H4 tests;
  - production USB Bluetooth class/control-compatibility/descriptor host tests;
  - clean ESP32-WROOM-32 production firmware build;
  - clean ESP32-S3 production firmware build;
  - both dedicated UART smoke-image builds; and
  - both WARN-only production release-profile builds with commit-addressed flash-input artifacts.
- The USB Bluetooth descriptor implementation uses the two-interface legacy Controller layout documented directly in the current SPEC and `docs/USB_BLUETOOTH_V1.md`.
- Static security/failure-semantics and release-configuration evidence is indexed in `docs/V1_EVIDENCE_INDEX.md`.
- V1-102 board selection is verified in `docs/V1_BOARD_VERIFICATION.md` from the project-owner purchase listings, prior board photographs/USB enumeration, and Espressif pin/native-USB documentation.
- The software portion of V1-1305 is audited in `docs/V1_DOCUMENTATION_EVIDENCE_AUDIT.md` and guarded by `scripts/check-doc-contract.sh` / Documentation CI.
- Hardware wiring/electrical tests and Windows/Linux driverless acceptance have **not** yet been performed and remain open.

---

# V1-000 — Repository and Toolchain Baseline

- [x] **V1-001 — Establish firmware directory structure**
  - [x] Create `firmware/esp32s3_usb_bridge/`.
  - [x] Create `firmware/esp32_wroom_bt_controller/`.
  - [x] Add shared components/configuration only where they reduce duplication cleanly.
  - [x] Keep each firmware target independently buildable and flashable.

- [x] **V1-002 — Select and pin the ESP-IDF baseline**
  - [x] Confirm one ESP-IDF release supports both ESP32 and ESP32-S3 requirements.
  - [x] Record the exact supported version in repository documentation.
  - [x] Add version/target guards so unsupported SDK/target combinations fail visibly.

- [x] **V1-003 — Define project-wide build conventions**
  - [x] Define warning policy.
  - [x] Define formatting/linting policy.
  - [x] Define reproducible `idf.py` build commands for both targets.
  - [x] Define clean-build verification.

- [x] **V1-004 — Add basic CI/build validation**
  - [x] Build WROOM firmware in CI.
  - [x] Build S3 firmware in CI.
  - [x] Fail on compile errors and project warning classes.
  - [x] Keep hardware-required acceptance separate from host-only CI.

**Gate V1-G00: PASS.** Run 31851995899 built both targets with ESP-IDF v5.5.5 and passed host transport tests.

---

# V1-100 — Hardware Contract and Bring-Up

- [x] **V1-101 — Record the reference interconnect**
  - [x] S3 GPIO4 TX -> WROOM GPIO16 RX2.
  - [x] S3 GPIO5 RX <- WROOM GPIO17 TX2.
  - [x] S3 GPIO6 RTS -> WROOM GPIO25 CTS.
  - [x] S3 GPIO7 CTS <- WROOM GPIO26 RTS.
  - [x] GND <-> GND.
  - [x] Document that TX/RX and RTS/CTS cross between endpoints.

- [x] **V1-102 — Verify selected development boards** — `docs/V1_BOARD_VERIFICATION.md`
  - [x] Identify the exact ESP32-S3 development board used for initial bring-up: AYWHP ESP32-S3-DevKitC-1-N16R8, ASIN `B0DG8L5NG5`.
  - [x] Confirm GPIO4-7 are usable and not hard-conflicted by board peripherals.
  - [x] Confirm the board exposes the S3 native USB device connection; prior project USB-device enumeration also exercised that path.
  - [x] Identify the exact ESP32-WROOM-32 development board used for bring-up: Aideepen 30-pin ESP-WROOM-32, ASIN `B0BQJ8BTVB`.
  - [x] Confirm GPIO16, GPIO17, GPIO25, and GPIO26 are accessible.

- [ ] **V1-103 — Verify electrical assumptions**
  - [x] Confirm both MCU GPIO domains use 3.3 V logic in the reference design.
  - [ ] Physically connect common ground.
  - [x] Document that independently regulated 3.3 V outputs must not be paralleled during prototype testing.
  - [x] Document prototype power arrangement.

- [ ] **V1-104 — Verify basic UART electrical communication**
  - [ ] Test S3 -> WROOM traffic.
  - [ ] Test WROOM -> S3 traffic.
  - [ ] Test RTS/CTS assertion and deassertion.
  - [ ] Verify no boot-strap/pin-state conflict during reset.

**Gate V1-G10: OPEN — selected boards are verified; physical common-ground, UART, RTS/CTS, and reset-state testing remain deferred.**

---

# V1-200 — HCI H4 Transport Core

- [x] **V1-201 — Define shared H4 packet model**
  - [x] Define command/event/ACL/SCO H4 packet type constants.
  - [x] Define command, event, ACL, and SCO framing/limits.
  - [x] Define explicit V1 SCO capability handling: parser model recognizes SCO, production bridge rejects it because V1 disables synchronous connections.

- [x] **V1-202 — Implement bounded H4 parsing**
  - [x] Parse packet type before type-specific length handling.
  - [x] Validate declared packet lengths.
  - [x] Reject impossible or oversized lengths.
  - [x] Reject malformed/truncated/trailing data without silently resynchronizing mid-frame.
  - [x] Avoid unbounded dynamic allocation in the steady-state forwarding path.

- [x] **V1-203 — Implement transport queues**
  - [x] Independent direction-specific queues.
  - [x] Explicit queue capacity constants.
  - [x] Queue high-water counters.
  - [x] Queue-full counters.
  - [x] No silent oldest/newest HCI packet dropping.

- [ ] **V1-204 — Implement hardware flow control policy**
  - [x] Enable UART RTS/CTS on S3.
  - [x] Enable UART RTS/CTS on WROOM.
  - [ ] Verify physical backpressure before receive exhaustion.
  - [ ] Verify forwarding resumes after physical queue pressure clears.

- [x] **V1-205 — Make baud rate a shared configuration**
  - [x] Start at conservative 115200 baud.
  - [x] Prevent source-level S3/WROOM baud mismatch with one shared definition.
  - [x] Document that the final rate is increased only after measured hardware stress testing.

- [x] **V1-206 — Add H4 transport tests**
  - [x] Valid command packet.
  - [x] Valid event packet.
  - [x] Valid ACL packet.
  - [x] Valid SCO model packet.
  - [x] Back-to-back packets.
  - [x] Fragmented input delivery.
  - [x] Invalid packet type.
  - [x] Oversized length.
  - [x] Truncated packet.
  - [x] Trailing-byte rejection.
  - [x] Queue exhaustion behavior.

**Gate V1-G20: PASS.** Strict host regression suite passes in CI and locally; parser/queue primitives are bounded and packet-preserving.

---

# V1-300 — ESP32-WROOM-32 Bluetooth Controller Firmware

- [x] **V1-301 — Create WROOM controller application**
  - [x] Target original ESP32.
  - [x] Configure BR/EDR + BLE dual-mode controller support.
  - [x] Use controller-only/VHCI mode; no application-level Bluetooth profiles.

- [x] **V1-302 — Configure HCI UART**
  - [x] WROOM RX GPIO16.
  - [x] WROOM TX GPIO17.
  - [x] WROOM CTS GPIO25 / RTS GPIO26 according to the reference crossing.
  - [x] Preserve UART0 for normal development flashing/logging where practical.
  - [x] Enable hardware flow control.

- [x] **V1-303 — Expose controller traffic through H4**
  - [x] Forward inbound HCI commands to VHCI controller.
  - [x] Forward controller events toward S3.
  - [x] Forward ACL data bidirectionally.
  - [x] Handle SCO/eSCO explicitly as out of V1 scope rather than exposing pseudo-support.

- [x] **V1-304 — Define WROOM boot state machine**
  - [x] reset/start state.
  - [x] controller initialization state.
  - [x] transport-ready state.
  - [x] operational state.
  - [x] fatal/recovering state.
  - [x] development diagnostics for transitions.

- [x] **V1-305 — Implement WROOM error handling**
  - [x] Detect controller initialization failure.
  - [x] Detect UART overflow/frame/parity/driver errors.
  - [x] Detect invalid inbound H4 framing.
  - [x] Fail closed on transport corruption.
  - [x] Define controlled recovery as deterministic MCU/controller restart.

- [ ] **V1-306 — Verify raw controller operation**
  - [ ] Send a basic HCI command over the physical inter-MCU link.
  - [ ] Receive the corresponding HCI event.
  - [ ] Read controller identity/version information from hardware.
  - [ ] Confirm BR/EDR capability on the actual controller.
  - [ ] Confirm BLE capability on the actual controller.

- [ ] **V1-307 — Verify WROOM restart scenarios**
  - [ ] WROOM reboot while S3 remains powered.
  - [ ] S3 reboot while WROOM remains powered.
  - [ ] Repeated WROOM power/reset cycles.

**Gate V1-G30: OPEN — firmware builds; physical controller/H4 behavior still requires hardware evidence.**

---

# V1-400 — ESP32-S3 Native USB Bluetooth Device

- [x] **V1-401 — Create S3 USB bridge application**
  - [x] Target ESP32-S3.
  - [x] Use the native USB device stack rather than a USB-UART transport.
  - [x] Pin and integrate ESP TinyUSB/TinyUSB dependencies.

- [x] **V1-402 — Implement USB Bluetooth descriptor/transport set**
  - [x] Use standard Bluetooth Wireless Controller E0/01/01 identity.
  - [x] Implement the standard two-interface legacy Bluetooth Controller configuration.
  - [x] Interface 0 alternate setting 0 carries the primary HCI transport with 3 endpoints: event IN, ACL OUT, and ACL IN.
  - [x] Interface 1 alternate setting 0 is E0/01/01 with 0 endpoints and represents zero active voice channels.
  - [x] Implement HCI command class control transfers, including legacy single-function device-targeted request compatibility.
  - [x] Implement interrupt IN for HCI events.
  - [x] Implement bulk OUT for host-to-controller ACL.
  - [x] Implement bulk IN for controller-to-host ACL, including transfer termination handling.
  - [x] Do not advertise nonzero-bandwidth SCO alternate settings or isochronous endpoints because SCO/eSCO is explicitly outside V1 scope.

- [x] **V1-403 — Define USB identity strategy**
  - [x] Define development manufacturer/product strings.
  - [x] Generate a stable per-S3 serial from factory base MAC.
  - [x] Document development VID/PID `0xCAFE:0x4011`.
  - [x] Explicitly prohibit unauthorized production use of another manufacturer's VID/PID.

- [ ] **V1-404 — Validate enumeration descriptors**
  - [ ] Inspect full descriptors from a physical Linux host.
  - [ ] Verify endpoint directions/types/max packet sizes on the wire.
  - [ ] Verify configuration/interface numbering on the wire.
  - [ ] Verify repeated physical enumeration is stable.

- [ ] **V1-405 — Implement/verify S3 USB lifecycle state machine**
  - [x] boot/unconfigured/enumerating state.
  - [x] configured/operational state.
  - [x] suspended state.
  - [x] resumed state.
  - [x] disconnected/recovering state.
  - [ ] Verify clean recovery after real host USB reset/suspend/resume/replug.

**Gate V1-G40: OPEN — descriptor/class source and host tests pass; physical USB enumeration is still required.**

---

# V1-500 — S3 USB-to-HCI Bridge Integration

- [x] **V1-501 — Implement host-command path**
  - [x] Receive HCI commands from USB control transfers.
  - [x] Add H4 command type and forward through bounded UART queue.
  - [x] Keep USB service behind controller readiness probing during startup.

- [x] **V1-502 — Implement event path**
  - [x] Receive WROOM HCI events over UART.
  - [x] Validate H4 event framing/length.
  - [x] Forward through USB interrupt IN.

- [x] **V1-503 — Implement ACL host-to-controller path**
  - [x] Reassemble USB bulk OUT ACL payloads.
  - [x] Add/preserve H4 ACL framing.
  - [x] Queue/forward through UART with configured flow control.

- [x] **V1-504 — Implement ACL controller-to-host path**
  - [x] Receive WROOM ACL traffic.
  - [x] Validate H4 packet boundaries/lengths.
  - [x] Forward over USB bulk IN.

- [x] **V1-505 — Implement SCO/eSCO path or explicit limitation**
  - [x] Evaluate the pinned TinyUSB/controller combination.
  - [x] Record that the pinned stock TinyUSB BTH voice/ISO path is not suitable for reliable V1 end-to-end SCO forwarding.
  - [x] Disable synchronous BR/EDR connections on WROOM.
  - [x] Expose only the zero-endpoint interface 1 alt 0 required for zero voice bandwidth; no usable SCO endpoints.
  - [x] Document HFP/HSP voice as out of V1 scope while retaining Classic ACL profiles.

- [ ] **V1-506 — Implement bridge readiness sequencing**
  - [x] Probe real WROOM with HCI Reset and Read Local Version before normal USB installation.
  - [x] Do not fabricate successful controller responses when startup probe fails.
  - [x] Define fatal transport corruption/USB protocol errors to detach/restart rather than forward garbage.
  - [ ] Verify behavior when a physical WROOM disappears/resets after USB enumeration.

- [x] **V1-507 — Add bridge diagnostics**
  - [x] HCI packet counters by type/direction.
  - [x] UART error counters.
  - [x] queue high-water marks.
  - [x] queue-full counters.
  - [x] USB attach/detach/suspend/resume counters.
  - [x] recovery and unexpected-SCO counters.
  - [x] Keep diagnostics off the USB HCI data endpoints.

**Gate V1-G50: OPEN — complete source path exists, but a real USB -> S3 -> UART -> WROOM -> UART -> S3 -> USB round trip requires hardware.**

---

# V1-600 — Linux Driverless Bring-Up

- [ ] **V1-601 — Verify generic USB Bluetooth binding**
  - [ ] Plug S3 native USB into Linux.
  - [ ] Confirm standard kernel USB Bluetooth driver binds.
  - [ ] Confirm an HCI controller appears and BlueZ sees it.
  - [ ] Confirm no `btattach`, project kernel module, or project daemon is required.

- [ ] **V1-602 — Verify Linux BLE basics**
  - [ ] Scan, connect, perform representative GATT operation, disconnect/reconnect.

- [ ] **V1-603 — Verify Linux Classic basics**
  - [ ] Inquiry/discovery, pair, connect/disconnect, reconnect after replug.

- [ ] **V1-604 — Verify Linux HID**
  - [ ] Pair a Classic/Bluetooth HID keyboard or mouse and confirm actual input/reconnect.

- [ ] **V1-605 — Exercise high ACL load on Linux**
  - [ ] Run A2DP or another sustained ACL workload and inspect bridge counters for corruption/overflow.

**Gate V1-G60: OPEN — physical Linux acceptance required.**

---

# V1-700 — Windows Driverless Bring-Up

- [ ] **V1-701 — Verify automatic Windows binding**
  - [ ] Cold-plug into a supported Windows system.
  - [ ] Confirm Windows identifies the device as Bluetooth and binds its normal in-box stack.
  - [ ] Confirm no project INF/driver is installed.
  - [ ] Confirm Bluetooth appears in normal Windows UI.

- [ ] **V1-702 — Verify Windows BLE basics**
  - [ ] Scan, pair/connect, exercise representative BLE function, disconnect/reconnect.

- [ ] **V1-703 — Verify Windows Classic basics**
  - [ ] Discover, pair, connect/disconnect, reconnect after replug.

- [ ] **V1-704 — Verify Windows HID**
  - [ ] Pair a keyboard/mouse and verify real input and reboot/replug reconnect.

- [ ] **V1-705 — Exercise high ACL load on Windows**
  - [ ] Run A2DP or another sustained ACL workload and inspect bridge diagnostics.

**Gate V1-G70: OPEN — physical Windows acceptance required.**

---

# V1-800 — Recovery and Robustness

- [ ] **V1-801 — USB unplug/replug recovery**
  - [ ] 10 normal unplug/replug cycles on Linux.
  - [ ] 10 normal unplug/replug cycles on Windows.
  - [ ] No reflashing/manual recovery required.

- [ ] **V1-802 — Host reboot recovery**
  - [ ] Linux boots with dongle attached and Bluetooth becomes available normally.
  - [ ] Windows boots with dongle attached and Bluetooth becomes available normally.

- [ ] **V1-803 — WROOM reset recovery**
  - [ ] Reset WROOM while S3 remains powered.
  - [ ] Verify no stale/corrupt forwarding.
  - [ ] Verify controller service returns predictably.

- [ ] **V1-804 — S3 reset recovery**
  - [ ] Reset S3 while WROOM remains powered.
  - [ ] Verify USB re-enumeration and HCI re-synchronization.

- [ ] **V1-805 — UART corruption/error behavior**
  - [ ] Exercise hardware receive overflow/error path where practical.
  - [x] Exercise malformed/truncated/oversized H4 input in the host test harness.
  - [x] Confirm software parser rejects ambiguous/corrupt frames rather than silently forwarding them.

- [ ] **V1-806 — Queue pressure behavior**
  - [x] Verify fixed queue exhaustion is detected/counted in host tests.
  - [ ] Force high host-to-controller pressure on hardware.
  - [ ] Force high controller-to-host pressure on hardware.
  - [ ] Verify physical RTS/CTS prevents corruption and forwarding resumes after pressure clears.

- [ ] **V1-807 — Suspend/resume**
  - [ ] Linux suspend/resume with dongle attached.
  - [ ] Windows sleep/resume where supported.
  - [ ] Confirm Bluetooth recovers without reflashing.

**Gate V1-G80: OPEN — hardware recovery matrix required.**

---

# V1-900 — Performance and Stability

- [ ] **V1-901 — Select final UART baud rate**
  - [x] Start from one shared conservative 115200-baud baseline.
  - [ ] Measure baseline on hardware.
  - [ ] Test progressively higher rates and record errors/high-water marks.
  - [ ] Select/lock the fastest comfortably stable final rate.

- [ ] **V1-902 — Sustained traffic test**
  - [ ] At least 30 minutes representative ACL traffic.
  - [ ] Zero bridge-caused malformed H4 packets/uncontrolled queue growth.
  - [ ] Record throughput and queue high-water marks.

- [ ] **V1-903 — Multi-hour stability run**
  - [ ] Multi-hour active Linux run.
  - [ ] Multi-hour active Windows run.
  - [ ] Record resets, USB errors, UART errors, disconnects.

- [ ] **V1-904 — Logging load audit**
  - [ ] Verify debug logging cannot starve USB/UART under hardware traffic.
  - [x] Release configuration statically removes INFO/DEBUG/VERBOSE logging above WARN maximum/default and prohibits raw buffer/hex dumps in production trees.
  - [ ] Reduce/rate-limit additional timing-sensitive logs if physical testing proves necessary.

**Gate V1-G90: OPEN — real sustained traffic required.**

---

# V1-1000 — Bluetooth Compatibility Matrix

- [ ] **V1-1001 — BLE peripheral matrix**
  - [ ] Simple BLE GATT peripheral, repeated scanning, pairing/bonding where supported.

- [ ] **V1-1002 — Classic HID matrix**
  - [ ] Classic HID keyboard/mouse where available; record host/device models.

- [ ] **V1-1003 — Classic audio/ACL stress matrix**
  - [ ] A2DP endpoint where available; record stability/throughput limitations.

- [x] **V1-1004 — SCO/eSCO decision**
  - [x] Determine synchronous USB transport is not release-ready for V1.
  - [x] Document it explicitly as a V1 limitation/post-V1 item.
  - [x] Do not advertise a broken USB synchronous transport path.

- [ ] **V1-1005 — Cross-host regression matrix**
  - [ ] Re-test selected devices on Linux and Windows and separate host-stack differences from firmware defects.

**Gate V1-G100: OPEN — real BLE + Classic compatibility evidence required.**

---

# V1-1100 — Security, Identity, and Release Hygiene

- [x] **V1-1101 — USB identity review**
  - [x] Development VID/PID policy documented.
  - [x] Unauthorized production use of another manufacturer's VID/PID prohibited.
  - [x] Manufacturer/product strings intentional.
  - [x] Serial policy is stable and per-device from the S3 factory base MAC.

- [x] **V1-1102 — Input validation review**
  - [x] H4 length validation is bounded.
  - [x] USB transfer/reassembly lengths are statically bounded.
  - [x] Controller-originated malformed H4 packets are rejected safely.
  - [x] No arbitrary host/controller length triggers unbounded allocation.

- [x] **V1-1103 — Failure semantics review**
  - [x] No fabricated successful HCI responses.
  - [x] Corrupt H4 frames fail closed rather than being silently skipped mid-frame.
  - [x] Development recovery/error reasons and counters are diagnosable.

- [x] **V1-1104 — Release configuration review**
  - [x] Reduce verbose diagnostics not intended for release through WARN-only default/maximum logging configuration.
  - [x] Confirm release logging policy does not expose pairing/security material unnecessarily.
  - [x] Confirm development-only interfaces are documented/disabled as intended.
  - [x] Validate both production release profiles and upload hashed commit-addressed flash inputs in CI.

**Gate V1-G110: PASS for software configuration.** Release-profile CI and static policy review pass; V1-904 remains open for the physical timing/load audit under sustained traffic.

---

# V1-1200 — Documentation and User Experience

- [x] **V1-1201 — Write build instructions** — `docs/BUILDING.md`
  - [x] Toolchain installation/version, both build commands, clean-build instructions.

- [x] **V1-1202 — Write flashing instructions** — `docs/FLASHING.md`
  - [x] Both firmware targets and final S3 native-USB connector explained.

- [x] **V1-1203 — Write wiring guide** — `docs/HARDWARE.md`
  - [x] Selected boards, pin table/diagram, common-ground requirement, separate-regulator warning.

- [x] **V1-1204 — Write normal-use guide** — `docs/USAGE.md`
  - [x] Windows/Linux normal Bluetooth paths and zero project-specific host setup requirement documented.

- [x] **V1-1205 — Write troubleshooting guide** — `docs/TROUBLESHOOTING.md`
  - [x] Enumeration, initialization, HCI/UART, connectivity, sustained traffic, and diagnostic collection covered.

- [ ] **V1-1206 — Record known limitations** — `docs/LIMITATIONS.md`
  - [x] SCO/eSCO status recorded.
  - [x] Selected development-board status corrected and linked to V1-102 evidence.
  - [ ] Tested host OS versions/builds still require hardware acceptance.
  - [ ] Tested Bluetooth peripherals still require hardware acceptance.
  - [ ] Throughput/stability observations still require hardware acceptance.

**Gate V1-G120: OPEN — documentation exists and has a software audit; physical build/flash/use walk-through must still validate it.**

---

# V1-1300 — Final V1 Acceptance

- [ ] **V1-1301 — Reproduce both firmware builds from clean checkout**
  - [x] Clean WROOM build passes in CI.
  - [x] Clean S3 build passes in CI.
  - [x] Release pipeline produces commit-addressed artifact names/hashes/flash inputs as development evidence.
  - [ ] Final artifacts must be captured again from the exact hardware-qualified release commit.

- [ ] **V1-1302 — Clean Linux acceptance run**
  - [ ] Standard USB Bluetooth driver/BlueZ, BLE, Classic, HID, sustained traffic, replug/reboot all pass without project host software.

- [ ] **V1-1303 — Clean Windows acceptance run**
  - [ ] In-box Bluetooth stack/UI, BLE, Classic, HID, sustained traffic, replug/reboot all pass without project host software.

- [x] **V1-1304 — Verify V1 non-goals have not leaked into requirements**
  - [x] No Wi-Fi implementation required for V1.
  - [x] No RTL8188EU emulation required for V1.
  - [x] No custom host Bluetooth driver required.
  - [x] No host helper/daemon required for ordinary Bluetooth operation.
  - [x] Bring-up smoke firmware is explicitly development-only and excluded from production component discovery.

- [ ] **V1-1305 — Documentation/evidence audit** — `docs/V1_DOCUMENTATION_EVIDENCE_AUDIT.md`
  - [x] All completed software gates/tasks have traceable implementation/test/CI/documentation evidence recorded or indexed.
  - [x] SPEC/TODO have been re-audited against the current software implementation and reconciled for locked SDK, board, USB, SCO, release, and non-goal decisions.
  - [x] Known limitations accurately distinguish selected-board completion from deferred physical validation.
  - [x] No known open V1 blocker is hidden by a development-only host path, diagnostic firmware, or component dependency.
  - [x] Add a lightweight Documentation CI contract check to prevent the stale normative states found by this audit from returning.
  - [ ] Re-audit SPEC/TODO/limitations against **actual hardware-tested behavior** after device testing resumes.

**V1-1305 software audit: PASS; parent remains OPEN only for the post-device-test behavior audit.**

- [ ] **V1-1306 — Declare V1 complete**
  - [ ] Mandatory tasks complete/superseded by documented spec change.
  - [ ] Linux acceptance passes.
  - [ ] Windows acceptance passes.
  - [ ] V1 release notes written.

**Gate V1-FINAL: OPEN.** Final acceptance remains: flash both production firmwares, connect the documented hardware, plug the S3 native USB port into Windows or Linux, and use Bluetooth Classic + BLE through the OS normal Bluetooth stack without ESP32 Radio Dongle host software.

---

# V2 — Deferred Wi-Fi Work

These are intentionally **not V1 tasks** and must not block V1 completion.

- [ ] **V2-001 — Re-evaluate USB resource budget after V1 is stable**
- [ ] **V2-002 — Prototype ESP32-S3 Wi-Fi while retaining V1 Bluetooth bridge operation**
- [ ] **V2-003 — Investigate RTL8188EU-compatible USB enumeration/driver binding**
- [ ] **V2-004 — Determine minimum observable RTL8188EU behavior required by existing host drivers**
- [ ] **V2-005 — Evaluate semantic translation from emulated Realtek behavior to ESP-IDF Wi-Fi APIs**
- [ ] **V2-006 — Evaluate Linux and Windows Wi-Fi compatibility separately**
- [ ] **V2-007 — Measure Bluetooth/Wi-Fi RF coexistence with the two physical 2.4 GHz radios**
- [ ] **V2-008 — Preserve all V1 driverless Bluetooth acceptance criteria while Wi-Fi is enabled**

No V2 task should alter the V1 reference inter-MCU HCI link unless a documented hardware revision is deliberately approved.
