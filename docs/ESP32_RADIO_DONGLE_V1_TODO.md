# ESP32 Radio Dongle V1 TODO

This TODO implements `docs/ESP32_RADIO_DONGLE_V1_SPEC.md`.

V1 is complete only when the device behaves as a normal USB Bluetooth Classic + BLE adapter on Windows and Linux with **no project-specific host-side software or custom Bluetooth driver required for normal operation**.

Wi-Fi is deferred to V2.

---

## Status Legend

- `[ ]` not complete
- `[x]` complete

Do not mark a task complete based only on code existing. Mark it complete only when its stated verification/evidence also passes.

---

# V1-000 — Repository and Toolchain Baseline

- [ ] **V1-001 — Establish firmware directory structure**
  - [ ] Create `firmware/esp32s3_usb_bridge/`.
  - [ ] Create `firmware/esp32_wroom_bt_controller/`.
  - [ ] Add shared components/configuration only where they reduce duplication cleanly.
  - [ ] Keep each firmware target independently buildable and flashable.

- [ ] **V1-002 — Select and pin the ESP-IDF baseline**
  - [ ] Confirm one ESP-IDF release supports both ESP32 and ESP32-S3 requirements.
  - [ ] Record the exact supported version in repository documentation.
  - [ ] Add version checks or clear build instructions so unsupported SDK versions fail visibly.

- [ ] **V1-003 — Define project-wide build conventions**
  - [ ] Define warning policy.
  - [ ] Define formatting/linting policy.
  - [ ] Define reproducible `idf.py` build commands for both targets.
  - [ ] Define clean-build verification.

- [ ] **V1-004 — Add basic CI/build validation**
  - [ ] Build WROOM firmware in CI or an equivalent reproducible environment.
  - [ ] Build S3 firmware in CI or an equivalent reproducible environment.
  - [ ] Fail on compile errors and selected warning classes.
  - [ ] Keep hardware-required acceptance separate from host-only CI.

**Gate V1-G00:** both empty/minimal firmware targets build reproducibly with the pinned toolchain.

---

# V1-100 — Hardware Contract and Bring-Up

- [ ] **V1-101 — Record the reference interconnect**
  - [ ] S3 GPIO4 TX -> WROOM GPIO16 RX2.
  - [ ] S3 GPIO5 RX <- WROOM GPIO17 TX2.
  - [ ] S3 GPIO6 RTS -> WROOM GPIO25 CTS.
  - [ ] S3 GPIO7 CTS <- WROOM GPIO26 RTS.
  - [ ] GND <-> GND.
  - [ ] Document that TX/RX and RTS/CTS cross between endpoints.

- [ ] **V1-102 — Verify selected development boards**
  - [ ] Identify the exact ESP32-S3 development board used for initial bring-up.
  - [ ] Confirm GPIO4-7 are usable and not hard-conflicted by board peripherals.
  - [ ] Confirm the board exposes the S3 native USB device connection.
  - [ ] Identify the exact ESP32-WROOM-32 development board used for bring-up.
  - [ ] Confirm GPIO16, GPIO17, GPIO25, and GPIO26 are accessible.

- [ ] **V1-103 — Verify electrical assumptions**
  - [ ] Confirm 3.3 V logic compatibility between boards.
  - [ ] Connect common ground.
  - [ ] Do not parallel independently regulated 3.3 V outputs during prototype testing.
  - [ ] Document prototype power arrangement.

- [ ] **V1-104 — Verify basic UART electrical communication**
  - [ ] Test S3 -> WROOM traffic.
  - [ ] Test WROOM -> S3 traffic.
  - [ ] Test RTS/CTS assertion and deassertion.
  - [ ] Verify no boot-strap/pin-state conflict during reset.

**Gate V1-G10:** both boards boot reliably with the reference wiring and pass bidirectional UART plus flow-control smoke tests.

---

# V1-200 — HCI H4 Transport Core

- [ ] **V1-201 — Define shared H4 packet model**
  - [ ] Define supported H4 packet type constants.
  - [ ] Define HCI command framing.
  - [ ] Define HCI event framing.
  - [ ] Define ACL framing.
  - [ ] Define SCO/eSCO framing path or explicit build-time capability handling.

- [ ] **V1-202 — Implement bounded H4 parsing**
  - [ ] Parse packet type before length-specific header handling.
  - [ ] Validate declared packet lengths.
  - [ ] Reject impossible or oversized lengths.
  - [ ] Reject malformed/truncated packets without leaking bytes into the next frame.
  - [ ] Avoid unbounded dynamic allocation in the steady-state forwarding path.

- [ ] **V1-203 — Implement transport queues**
  - [ ] Independent host-to-controller queue.
  - [ ] Independent controller-to-host queue.
  - [ ] Explicit queue capacity constants.
  - [ ] Queue high-water counters.
  - [ ] Queue-full counters.
  - [ ] No silent oldest/newest packet dropping.

- [ ] **V1-204 — Implement hardware flow control policy**
  - [ ] Enable UART RTS/CTS on S3.
  - [ ] Enable UART RTS/CTS on WROOM.
  - [ ] Verify backpressure before receive queue exhaustion.
  - [ ] Verify resume after queue pressure clears.

- [ ] **V1-205 — Make baud rate a shared configuration**
  - [ ] Start with a conservative bring-up rate.
  - [ ] Prevent accidental S3/WROOM baud mismatch.
  - [ ] Add a documented procedure for increasing the final rate.

- [ ] **V1-206 — Add H4 transport tests**
  - [ ] Valid command packet.
  - [ ] Valid event packet.
  - [ ] Valid ACL packet.
  - [ ] Valid SCO packet when supported.
  - [ ] Back-to-back packets.
  - [ ] Fragmented input delivery.
  - [ ] Invalid packet type.
  - [ ] Oversized length.
  - [ ] Truncated packet.
  - [ ] Queue exhaustion behavior.

**Gate V1-G20:** H4 forwarding primitives are bounded, tested, and preserve packet boundaries under fragmented and back-to-back traffic.

---

# V1-300 — ESP32-WROOM-32 Bluetooth Controller Firmware

- [ ] **V1-301 — Create WROOM controller application**
  - [ ] Set ESP-IDF target to original ESP32.
  - [ ] Configure BR/EDR + BLE dual-mode controller support.
  - [ ] Avoid application-level Bluetooth profiles in V1 controller firmware.

- [ ] **V1-302 — Configure HCI UART**
  - [ ] Use GPIO16 as WROOM receive path.
  - [ ] Use GPIO17 as WROOM transmit path.
  - [ ] Use GPIO25/26 for CTS/RTS according to the reference crossing.
  - [ ] Preserve UART0 for development logging/flashing where practical.
  - [ ] Enable hardware flow control.

- [ ] **V1-303 — Expose controller traffic through H4**
  - [ ] Forward inbound HCI commands to the Bluetooth controller.
  - [ ] Forward controller events to S3.
  - [ ] Forward ACL data bidirectionally.
  - [ ] Add SCO/eSCO forwarding when controller support and V1 scope permit.

- [ ] **V1-304 — Define WROOM boot state machine**
  - [ ] Reset/start state.
  - [ ] controller initialization state.
  - [ ] transport-ready state.
  - [ ] operational state.
  - [ ] fatal/recovering state.
  - [ ] Emit development diagnostics for transitions.

- [ ] **V1-305 — Implement WROOM error handling**
  - [ ] Detect controller initialization failure.
  - [ ] Detect UART driver errors/overflow.
  - [ ] Detect invalid inbound H4 framing.
  - [ ] Fail closed on transport corruption.
  - [ ] Define controlled controller reinitialization behavior.

- [ ] **V1-306 — Verify raw controller operation**
  - [ ] Send a basic HCI command from a development harness.
  - [ ] Receive the corresponding HCI event.
  - [ ] Read controller identity/version information.
  - [ ] Confirm BR/EDR controller capability is present.
  - [ ] Confirm BLE controller capability is present.

- [ ] **V1-307 — Verify WROOM restart scenarios**
  - [ ] WROOM reboot while S3 remains powered.
  - [ ] S3 reboot while WROOM remains powered.
  - [ ] Repeated WROOM power/reset cycles.

**Gate V1-G30:** the WROOM behaves as a stable dual-mode Bluetooth controller reachable over the documented H4 UART link.

---

# V1-400 — ESP32-S3 Native USB Bluetooth Device

- [ ] **V1-401 — Create S3 USB bridge application**
  - [ ] Set ESP-IDF target to ESP32-S3.
  - [ ] Use the native USB peripheral, not a USB-UART bridge.
  - [ ] Integrate the chosen TinyUSB/ESP-IDF USB device path.

- [ ] **V1-402 — Implement USB Bluetooth descriptor set**
  - [ ] Use the standard Bluetooth wireless-controller class/subclass/protocol identity expected by host Bluetooth USB support.
  - [ ] Implement the required interface descriptors.
  - [ ] Implement HCI command control-transfer handling.
  - [ ] Implement interrupt IN endpoint for HCI events.
  - [ ] Implement bulk OUT endpoint for host-to-controller ACL.
  - [ ] Implement bulk IN endpoint for controller-to-host ACL.
  - [ ] Add appropriate isochronous endpoints if SCO/eSCO is in the V1 release scope.

- [ ] **V1-403 — Define USB identity strategy**
  - [ ] Define development manufacturer string.
  - [ ] Define development product string.
  - [ ] Define serial-number generation/assignment strategy.
  - [ ] Document development VID/PID use.
  - [ ] Explicitly prohibit production use of another manufacturer's VID/PID.

- [ ] **V1-404 — Validate enumeration descriptors**
  - [ ] Inspect full descriptors on Linux.
  - [ ] Verify endpoint directions/types/max packet sizes.
  - [ ] Verify configuration and interface numbering.
  - [ ] Verify repeated enumeration is stable.

- [ ] **V1-405 — Implement S3 USB lifecycle state machine**
  - [ ] boot/unconfigured state.
  - [ ] configured state.
  - [ ] suspended state.
  - [ ] resumed state.
  - [ ] disconnected/reset state.
  - [ ] clean recovery after host USB reset.

**Gate V1-G40:** S3 repeatedly enumerates with a structurally valid standard USB Bluetooth interface before the full WROOM bridge is enabled.

---

# V1-500 — S3 USB-to-HCI Bridge Integration

- [ ] **V1-501 — Implement host-command path**
  - [ ] Receive HCI commands from USB control transfers.
  - [ ] Frame/forward commands over H4 UART.
  - [ ] Prevent forwarding until controller transport is ready.

- [ ] **V1-502 — Implement event path**
  - [ ] Receive WROOM HCI events over UART.
  - [ ] Validate event framing/length.
  - [ ] Forward through USB interrupt IN.

- [ ] **V1-503 — Implement ACL host-to-controller path**
  - [ ] Receive USB bulk OUT ACL payloads.
  - [ ] Add/preserve required H4 framing.
  - [ ] Queue and forward using UART flow control.

- [ ] **V1-504 — Implement ACL controller-to-host path**
  - [ ] Receive WROOM ACL traffic.
  - [ ] Validate packet boundaries.
  - [ ] Forward over USB bulk IN.

- [ ] **V1-505 — Implement SCO/eSCO path or explicit limitation**
  - [ ] Determine whether the selected ESP-IDF/TinyUSB/controller combination supports the required synchronous transport reliably.
  - [ ] If supported, implement and test both directions.
  - [ ] If not supported for V1, document the limitation explicitly and ensure no malformed pseudo-support is exposed.

- [ ] **V1-506 — Implement bridge readiness sequencing**
  - [ ] Do not report an operational controller before UART/controller readiness.
  - [ ] Define behavior when WROOM becomes unavailable after USB enumeration.
  - [ ] Reinitialize or detach/reset cleanly rather than forwarding garbage.

- [ ] **V1-507 — Add bridge diagnostics**
  - [ ] HCI packet counters by type/direction.
  - [ ] UART error counters.
  - [ ] queue high-water marks.
  - [ ] queue-full counters.
  - [ ] USB reset/suspend/resume counters.
  - [ ] controller reset/recovery counter.
  - [ ] Keep diagnostics off the HCI data path.

**Gate V1-G50:** a host HCI command traverses USB -> S3 -> UART -> WROOM and its valid response returns WROOM -> UART -> S3 -> USB without a host-side serial helper.

---

# V1-600 — Linux Driverless Bring-Up

- [ ] **V1-601 — Verify generic USB Bluetooth binding**
  - [ ] Plug the S3 native USB connection into Linux.
  - [ ] Confirm the standard kernel USB Bluetooth driver binds.
  - [ ] Confirm an HCI controller appears.
  - [ ] Confirm BlueZ sees the controller.
  - [ ] Confirm no `btattach` step is required.
  - [ ] Confirm no project-specific kernel module is required.
  - [ ] Confirm no project-specific daemon is required.

- [ ] **V1-602 — Verify Linux BLE basics**
  - [ ] Scan for BLE devices.
  - [ ] Connect to a representative BLE peripheral.
  - [ ] Exercise a representative GATT operation.
  - [ ] Disconnect/reconnect.

- [ ] **V1-603 — Verify Linux Classic basics**
  - [ ] Classic inquiry/discovery.
  - [ ] Pair a representative Classic device.
  - [ ] Connect/disconnect.
  - [ ] Reconnect after USB replug.

- [ ] **V1-604 — Verify Linux HID**
  - [ ] Pair a Bluetooth keyboard or mouse.
  - [ ] Confirm actual input events reach Linux.
  - [ ] Verify reconnect after host/dongle restart.

- [ ] **V1-605 — Exercise high ACL load on Linux**
  - [ ] Use A2DP or another representative sustained ACL workload where practical.
  - [ ] Observe queue high-water marks.
  - [ ] Confirm no HCI framing corruption.
  - [ ] Confirm no unexpected UART overflow.

**Gate V1-G60:** Linux uses the dongle through its standard USB Bluetooth/BlueZ path with no project-specific host software.

---

# V1-700 — Windows Driverless Bring-Up

- [ ] **V1-701 — Verify automatic Windows binding**
  - [ ] Cold-plug device into a supported Windows system.
  - [ ] Confirm Windows identifies it as a Bluetooth adapter/controller.
  - [ ] Confirm the normal in-box Bluetooth USB stack binds.
  - [ ] Confirm no project-specific INF/driver package is installed.
  - [ ] Confirm Bluetooth appears in normal Windows settings/device UI.

- [ ] **V1-702 — Verify Windows BLE basics**
  - [ ] Scan for BLE devices.
  - [ ] Pair/connect to a representative BLE peripheral.
  - [ ] Exercise representative BLE functionality.
  - [ ] Disconnect/reconnect.

- [ ] **V1-703 — Verify Windows Classic basics**
  - [ ] Discover a representative Classic device.
  - [ ] Pair.
  - [ ] Connect/disconnect.
  - [ ] Reconnect after USB replug.

- [ ] **V1-704 — Verify Windows HID**
  - [ ] Pair a Bluetooth keyboard or mouse.
  - [ ] Confirm actual input reaches Windows.
  - [ ] Verify reconnect after reboot/replug.

- [ ] **V1-705 — Exercise high ACL load on Windows**
  - [ ] Use A2DP or another representative sustained ACL workload where practical.
  - [ ] Observe bridge diagnostics.
  - [ ] Confirm no HCI framing corruption.
  - [ ] Confirm no unexpected UART overflow.

**Gate V1-G70:** Windows uses the dongle through its normal in-box Bluetooth stack with no project-specific host software or custom Bluetooth driver.

---

# V1-800 — Recovery and Robustness

- [ ] **V1-801 — USB unplug/replug recovery**
  - [ ] 10 consecutive normal unplug/replug cycles on Linux.
  - [ ] 10 consecutive normal unplug/replug cycles on Windows.
  - [ ] No reflashing/reset procedure required.

- [ ] **V1-802 — Host reboot recovery**
  - [ ] Linux boots with dongle already attached.
  - [ ] Windows boots with dongle already attached.
  - [ ] Bluetooth becomes available normally after boot.

- [ ] **V1-803 — WROOM reset recovery**
  - [ ] Reset WROOM while S3 remains powered.
  - [ ] Verify S3 does not forward corrupted/stale packets.
  - [ ] Verify controller service recovers predictably.

- [ ] **V1-804 — S3 reset recovery**
  - [ ] Reset S3 while WROOM remains powered.
  - [ ] Verify USB re-enumeration.
  - [ ] Verify HCI transport re-synchronization.

- [ ] **V1-805 — UART corruption/error behavior**
  - [ ] Exercise receive overflow/error path where practical.
  - [ ] Exercise malformed H4 input using a test harness.
  - [ ] Confirm transport resets/rejects rather than forwarding ambiguous bytes.

- [ ] **V1-806 — Queue pressure behavior**
  - [ ] Force high host-to-controller pressure.
  - [ ] Force high controller-to-host pressure.
  - [ ] Verify RTS/CTS/backpressure activates before corruption.
  - [ ] Verify queues drain and forwarding resumes.

- [ ] **V1-807 — Suspend/resume**
  - [ ] Linux suspend/resume with dongle attached.
  - [ ] Windows sleep/resume with dongle attached where test hardware permits.
  - [ ] Confirm Bluetooth recovers without reflashing.

**Gate V1-G80:** all defined recovery paths preserve framing and return the device to usable service without project-specific host intervention.

---

# V1-900 — Performance and Stability

- [ ] **V1-901 — Select final UART baud rate**
  - [ ] Measure baseline at conservative baud.
  - [ ] Test progressively higher supported rates.
  - [ ] Record error/overflow/high-water statistics.
  - [ ] Select the fastest rate that remains comfortably stable.
  - [ ] Lock both firmware targets to one shared/default value.

- [ ] **V1-902 — Sustained traffic test**
  - [ ] Run representative sustained ACL traffic for at least 30 minutes.
  - [ ] Verify zero malformed H4 packets caused by the bridge.
  - [ ] Verify no uncontrolled queue growth.
  - [ ] Record throughput and queue high-water marks.

- [ ] **V1-903 — Multi-hour stability run**
  - [ ] Run at least one multi-hour Linux stability session.
  - [ ] Run at least one multi-hour Windows stability session.
  - [ ] Include active Bluetooth traffic, not idle-only testing.
  - [ ] Record resets, USB errors, UART errors, and disconnects.

- [ ] **V1-904 — Logging load audit**
  - [ ] Verify debug logging cannot starve USB handling.
  - [ ] Verify debug logging cannot starve UART handling.
  - [ ] Reduce or rate-limit logs that affect timing.

**Gate V1-G90:** final transport configuration is stable under sustained real Bluetooth traffic on both host operating systems.

---

# V1-1000 — Bluetooth Compatibility Matrix

- [ ] **V1-1001 — BLE peripheral matrix**
  - [ ] Test at least one simple BLE GATT peripheral.
  - [ ] Test repeated scanning.
  - [ ] Test pairing/bonding where supported by the peripheral.

- [ ] **V1-1002 — Classic HID matrix**
  - [ ] Test at least one Bluetooth Classic HID keyboard or mouse if available.
  - [ ] Record host OS and device model.

- [ ] **V1-1003 — Classic audio/ACL stress matrix**
  - [ ] Test at least one A2DP audio endpoint if available.
  - [ ] Record stability and any throughput limitation.

- [ ] **V1-1004 — SCO/eSCO decision**
  - [ ] Test synchronous transport if implemented.
  - [ ] If not release-ready, document it explicitly as a known V1 limitation/post-V1 item.
  - [ ] Ensure the host interface does not falsely advertise a broken transport path.

- [ ] **V1-1005 — Cross-host regression matrix**
  - [ ] Re-test selected devices on Linux.
  - [ ] Re-test selected devices on Windows.
  - [ ] Record differences caused by host stack behavior separately from firmware defects.

**Gate V1-G100:** the compatibility matrix demonstrates both BLE and Bluetooth Classic operation on Windows and Linux, with all known profile/transport limitations documented.

---

# V1-1100 — Security, Identity, and Release Hygiene

- [ ] **V1-1101 — USB identity review**
  - [ ] Verify development VID/PID policy is documented.
  - [ ] Verify no production claim uses another manufacturer's VID/PID without authorization.
  - [ ] Verify manufacturer/product strings are intentional.
  - [ ] Verify serial number is stable/unique according to project policy.

- [ ] **V1-1102 — Input validation review**
  - [ ] H4 length validation cannot overflow buffers.
  - [ ] USB transfer lengths are bounded.
  - [ ] Controller-originated malformed lengths are rejected safely.
  - [ ] No arbitrary packet length can trigger unbounded allocation.

- [ ] **V1-1103 — Failure semantics review**
  - [ ] Do not fabricate successful HCI responses.
  - [ ] Do not silently discard corrupt packets and continue mid-frame.
  - [ ] Reset/recovery reasons are diagnosable during development.

- [ ] **V1-1104 — Release configuration review**
  - [ ] Disable or reduce verbose diagnostics not intended for release.
  - [ ] Confirm release logging cannot leak pairing/security material unnecessarily.
  - [ ] Confirm development-only interfaces are documented or disabled as intended.

**Gate V1-G110:** release firmware has bounded parsing, intentional USB identity, and no known fail-open HCI behavior.

---

# V1-1200 — Documentation and User Experience

- [ ] **V1-1201 — Write build instructions**
  - [ ] Toolchain installation/version.
  - [ ] WROOM build command.
  - [ ] S3 build command.
  - [ ] Clean build instructions.

- [ ] **V1-1202 — Write flashing instructions**
  - [ ] Flash WROOM firmware.
  - [ ] Flash S3 firmware.
  - [ ] Explain which USB connector is the final host-facing native USB connection.

- [ ] **V1-1203 — Write wiring guide**
  - [ ] Include reference pin table.
  - [ ] Include ASCII or image wiring diagram.
  - [ ] Explain common ground.
  - [ ] Warn not to parallel independently regulated 3.3 V rails during the dev-board prototype.

- [ ] **V1-1204 — Write normal-use guide**
  - [ ] Windows: plug in and use normal Bluetooth settings.
  - [ ] Linux: plug in and use normal desktop Bluetooth UI/BlueZ tools.
  - [ ] Explicitly state that no project-specific host setup is required.

- [ ] **V1-1205 — Write troubleshooting guide**
  - [ ] USB does not enumerate.
  - [ ] Bluetooth adapter appears but controller initialization fails.
  - [ ] UART/HCI synchronization issue.
  - [ ] Pairing/connectivity issue.
  - [ ] How to collect firmware diagnostics without changing normal host requirements.

- [ ] **V1-1206 — Record known limitations**
  - [ ] SCO/eSCO status.
  - [ ] Tested host OS versions.
  - [ ] Tested Bluetooth peripherals.
  - [ ] Throughput/stability observations.

**Gate V1-G120:** a new user can build, flash, wire, and use V1 from repository documentation without undocumented project-specific host software.

---

# V1-1300 — Final V1 Acceptance

- [ ] **V1-1301 — Reproduce both firmware builds from clean checkout**
  - [ ] Clean WROOM build passes.
  - [ ] Clean S3 build passes.
  - [ ] Produced artifacts are identified/documented.

- [ ] **V1-1302 — Clean Linux acceptance run**
  - [ ] Plug into a Linux host that has not installed project-specific software.
  - [ ] Standard USB Bluetooth driver binds.
  - [ ] BlueZ sees controller.
  - [ ] BLE scan/pair/connect passes.
  - [ ] Classic scan/pair/connect passes.
  - [ ] HID passes.
  - [ ] Sustained traffic passes.
  - [ ] Replug/reboot recovery passes.

- [ ] **V1-1303 — Clean Windows acceptance run**
  - [ ] Plug into a Windows host that has not installed project-specific ESP32 Radio Dongle software.
  - [ ] Normal in-box Bluetooth stack binds automatically.
  - [ ] Bluetooth appears in normal Windows UI.
  - [ ] BLE scan/pair/connect passes.
  - [ ] Classic scan/pair/connect passes.
  - [ ] HID passes.
  - [ ] Sustained traffic passes.
  - [ ] Replug/reboot recovery passes.

- [ ] **V1-1304 — Verify V1 non-goals have not leaked into requirements**
  - [ ] No Wi-Fi implementation is required for V1.
  - [ ] No RTL8188EU emulation is required for V1.
  - [ ] No custom host driver is required.
  - [ ] No host helper/daemon is required for ordinary Bluetooth operation.

- [ ] **V1-1305 — Documentation/evidence audit**
  - [ ] All completed gates have evidence recorded.
  - [ ] All known limitations are documented.
  - [ ] Spec and TODO match the shipped behavior.
  - [ ] No open blocker is hidden by a development-only workaround.

- [ ] **V1-1306 — Declare V1 complete**
  - [ ] All mandatory tasks above are complete or explicitly superseded by a documented spec change.
  - [ ] Linux acceptance passes.
  - [ ] Windows acceptance passes.
  - [ ] V1 release notes are written.

**Gate V1-FINAL:** flash both firmwares, connect the documented hardware, plug the ESP32-S3 native USB port into Windows or Linux, and use Bluetooth Classic + BLE through the operating system's normal Bluetooth stack without installing any ESP32 Radio Dongle host software.

---

# V2 — Deferred Wi-Fi Work

The following items are intentionally **not V1 tasks** and must not block V1 completion.

- [ ] **V2-001 — Re-evaluate USB resource budget after V1 is stable**
- [ ] **V2-002 — Prototype ESP32-S3 Wi-Fi while retaining V1 Bluetooth bridge operation**
- [ ] **V2-003 — Investigate RTL8188EU-compatible USB enumeration/driver binding**
- [ ] **V2-004 — Determine the minimum observable RTL8188EU behavior required by existing host drivers**
- [ ] **V2-005 — Evaluate semantic translation from emulated Realtek behavior to ESP-IDF Wi-Fi APIs**
- [ ] **V2-006 — Evaluate Linux and Windows Wi-Fi compatibility separately**
- [ ] **V2-007 — Measure Bluetooth/Wi-Fi RF coexistence with the two physical 2.4 GHz radios**
- [ ] **V2-008 — Preserve all V1 driverless Bluetooth acceptance criteria while Wi-Fi is enabled**

No V2 task should alter the V1 reference inter-MCU HCI link unless a documented hardware revision is deliberately approved.
