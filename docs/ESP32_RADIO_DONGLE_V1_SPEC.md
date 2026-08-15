# ESP32 Radio Dongle V1 Specification

Status: current V1 implementation and acceptance contract.

## 1. Purpose

ESP32 Radio Dongle V1 is a dual-MCU USB Bluetooth adapter built from:

- an **ESP32-S3** acting as the native USB device and USB-to-HCI bridge; and
- an **ESP32-WROOM-32** acting as the Bluetooth controller/radio for Bluetooth Classic (BR/EDR) and Bluetooth Low Energy (BLE).

The defining V1 product requirement is:

> After both firmware images are flashed and the hardware is connected, plugging the ESP32-S3 native USB port into a normal Windows or Linux computer must make the device appear as an ordinary USB Bluetooth adapter without installing project-specific host software, drivers, daemons, scripts, or helper applications.

Windows and Linux must use their normal Bluetooth stacks and USB Bluetooth support.

Wi-Fi is explicitly deferred to V2.

---

## 2. V1 Goals

V1 SHALL:

1. Provide Bluetooth Classic (BR/EDR) and BLE through the ESP32-WROOM-32 radio.
2. Present a standards-oriented USB Bluetooth HCI Primary Controller interface through the ESP32-S3 native USB peripheral.
3. Work with the normal Windows Bluetooth stack without a project-specific Windows Bluetooth driver or INF.
4. Work with the normal Linux USB Bluetooth/BlueZ stack without a project-specific Linux driver or serial-HCI attach utility.
5. Use a direct HCI transport between the two MCUs rather than running Bluetooth profiles on the ESP32-S3.
6. Use UART H4 with hardware RTS/CTS flow control between the ESP32-S3 and ESP32-WROOM-32.
7. Keep the ESP32-WROOM-32 UART0/USB-UART path available for development flashing/logging.
8. Recover cleanly from USB disconnect/reconnect, host reboot, controller reset, and transport-integrity failure.
9. Use bounded transport buffers and fail closed rather than forwarding ambiguous or corrupt HCI traffic.
10. Keep the architecture compatible with a later V2 that adds ESP32-S3 Wi-Fi without redesigning the V1 inter-MCU HCI link.

---

## 3. V1 Non-Goals and Explicit Limits

V1 SHALL NOT require or implement:

- Wi-Fi USB functionality.
- RTL8188EU or other USB Wi-Fi chipset emulation.
- A custom Windows Bluetooth driver.
- A custom Linux Bluetooth driver.
- A host-side setup utility for normal Bluetooth operation.
- A host-side daemon or background service.
- A web configuration UI.
- A custom PCB as a prerequisite for functional acceptance.
- Maximum possible UART or USB throughput optimization before correctness and stability.
- Bluetooth profiles implemented on the ESP32-S3.
- SCO/eSCO synchronous voice transport.
- HFP/HSP voice-audio acceptance.

V1 may include development-only diagnostics, scripts, test tools, logging, and dedicated bring-up firmware. Those assets must not be required for normal end-user Bluetooth operation.

Bluetooth Classic remains a V1 requirement despite the SCO/eSCO limitation. ACL-based Classic workloads such as HID and A2DP remain in scope.

---

## 4. System Architecture

```text
                         USB
                          |
                          v
                +-------------------+
                |     ESP32-S3      |
                |                   |
                | Native USB device |
                | USB Bluetooth HCI |
                | HCI bridge        |
                +---------+---------+
                          |
                    HCI H4 UART
                  TX / RX / RTS / CTS
                          |
                +---------v---------+
                | ESP32-WROOM-32    |
                |                   |
                | BT controller     |
                | BR/EDR + BLE      |
                +---------+---------+
                          |
                          v
                    2.4 GHz radio
```

### 4.1 Host responsibility

The Windows or Linux host owns the Bluetooth host stack, pairing policy, and profiles. Examples include HID, A2DP, GATT, and other host-side profile behavior.

### 4.2 ESP32-S3 responsibility

The ESP32-S3 owns:

- native USB enumeration;
- USB Bluetooth HCI transport;
- HCI packet adaptation/forwarding;
- UART flow control and buffering;
- controller readiness supervision;
- bridge diagnostics and fail-closed recovery; and
- future V2 USB expansion subject to preserving V1 behavior.

The S3 SHALL NOT terminate normal Bluetooth profile semantics.

### 4.3 ESP32-WROOM-32 responsibility

The ESP32-WROOM-32 owns:

- Bluetooth controller initialization;
- BR/EDR radio/controller operation;
- BLE radio/controller operation;
- standard HCI controller behavior; and
- HCI H4 transport to the ESP32-S3.

The WROOM SHALL run controller-oriented firmware rather than application-level Bluetooth profiles for V1.

---

## 5. Hardware Contract

### 5.1 Selected V1 development boards

The initial V1 bring-up boards are verified in `docs/V1_BOARD_VERIFICATION.md`:

- ESP32-S3: AYWHP ESP32-S3-DevKitC-1-N16R8, purchase ASIN `B0DG8L5NG5`.
- Original ESP32: Aideepen 30-pin ESP-WROOM-32 development board, purchase ASIN `B0BQJ8BTVB`.

The S3 selection exposes GPIO4-7 and a native USB device path. The WROOM selection exposes GPIO16/17/25/26 and uses its CP210x USB-UART path only for development flashing/logging.

Substituting a different board requires rechecking board-specific pin/native-USB conflicts before physical testing.

### 5.2 V1 reference interconnect

This pin assignment is the V1 reference assignment unless deliberately revised:

| Signal | ESP32-S3 | Direction | ESP32-WROOM-32 |
| --- | ---: | :---: | ---: |
| HCI TX | GPIO4 | -> | GPIO16 / UART2 RX |
| HCI RX | GPIO5 | <- | GPIO17 / UART2 TX |
| HCI RTS | GPIO6 | -> | GPIO25 / UART2 CTS |
| HCI CTS | GPIO7 | <- | GPIO26 / UART2 RTS |
| Ground | GND | <-> | GND |

TX/RX and RTS/CTS are crossed between endpoints.

### 5.3 Electrical requirements

- Both MCU GPIO domains are 3.3 V for the selected prototype boards.
- During prototype development, the two boards may be independently USB-powered while sharing ground.
- Independently regulated 3.3 V outputs SHALL NOT be tied together.
- The reference prototype connects only TX, RX, RTS, CTS, and common ground between boards unless a later hardware revision explicitly adds another signal.
- A future integrated PCB should use one appropriately sized power tree from the USB input.

### 5.4 USB connector requirement

The host-facing connector must reach the **ESP32-S3 native USB peripheral**, not merely a USB-to-UART bridge.

The WROOM USB-UART connector is never the normal V1 Bluetooth host path.

---

## 6. Inter-MCU HCI Transport

### 6.1 Transport format

The inter-MCU transport SHALL use Bluetooth HCI UART Transport Layer H4 framing.

The shared parser/model recognizes the normal H4 packet types needed for validation, including Command, Event, ACL, and SCO model packets. The active V1 production forwarding contract is:

- HCI Command packets: S3 -> WROOM.
- HCI Event packets: WROOM -> S3.
- ACL packets: bidirectional.
- SCO packets: recognized as a packet type but **rejected as unexpected in the V1 no-SCO configuration**.

V1 SHALL NOT expose a working-looking but unserviceable SCO/eSCO transport path.

### 6.2 UART configuration

- Hardware UART SHALL be used on both processors.
- RTS/CTS hardware flow control SHALL be enabled for the production V1 data path.
- The UART baud rate SHALL come from one shared configuration definition so S3/WROOM source settings cannot intentionally diverge.
- The software baseline is 115200 baud.
- The final V1 baud rate SHALL be selected only after physical backpressure and sustained bidirectional stress testing.
- Firmware SHALL NOT silently continue with mismatched transport settings.

### 6.3 Buffering and framing

The bridge SHALL use bounded buffers and finite packet limits.

Requirements:

- No unbounded allocation in the steady-state HCI forwarding path.
- USB-to-UART and UART-to-USB paths have independent buffering.
- Flow control/backpressure must be applied before physical receive exhaustion.
- Queue exhaustion must be observable and treated as a transport-integrity failure rather than silent packet loss.
- HCI packets must never be partially discarded and then passed onward as if valid.
- Invalid packet type, impossible length, truncation, or framing corruption must fail closed.

### 6.4 Startup synchronization

The ESP32-S3 SHALL not intentionally expose normal USB Bluetooth service until the WROOM controller path has proved responsive.

The implemented startup contract is:

1. S3 boot.
2. H4 UART/RTS-CTS initialization.
3. HCI Reset sent to the WROOM.
4. Successful Command Complete required.
5. HCI Read Local Version Information sent.
6. Successful version response required.
7. Normal bridge tasks start.
8. Native USB Bluetooth service is installed/attached.

If the WROOM probe fails, the S3 SHALL NOT fabricate successful HCI responses merely to make the host enumerate a nominal controller.

---

## 7. ESP32-WROOM-32 Firmware

### 7.1 Role

The WROOM firmware SHALL run the original ESP32 Bluetooth subsystem as a dual-mode BR/EDR + BLE controller suitable for an external host stack.

### 7.2 Required implementation

The production WROOM firmware SHALL:

- target `esp32`;
- use ESP-IDF v5.5.5;
- configure BR/EDR + BLE controller-only operation;
- use VHCI between the ESP-IDF controller and project bridge logic;
- expose the external H4 link through application-owned UART2;
- route TX=GPIO17, RX=GPIO16, RTS=GPIO26, CTS=GPIO25;
- enable hardware flow control;
- keep UART0/CP210x available for development flashing/logging where practical;
- avoid application-level Bluetooth profiles;
- configure zero synchronous BR/EDR connections for V1;
- validate controller-originated H4 packets before queueing; and
- restart deterministically on fatal bridge/controller integrity failure.

### 7.3 Error/recovery contract

The WROOM path SHALL detect or fail safely on:

- controller initialization failure;
- UART receive overflow/frame/parity/driver errors;
- invalid inbound H4 traffic;
- queue exhaustion; and
- unexpected SCO traffic under the V1 configuration.

Physical reset/restart timing behavior remains an acceptance test and is not proven by source/CI alone.

---

## 8. ESP32-S3 Native USB Firmware

### 8.1 Role

The ESP32-S3 SHALL present the assembled device as a normal USB Bluetooth Primary Controller while forwarding HCI traffic to/from the WROOM.

The production S3 firmware SHALL target `esp32s3` and ESP-IDF v5.5.5.

### 8.2 Host-visible USB identity

The device and Bluetooth interfaces use:

```text
Class:    0xE0  Wireless Controller
Subclass: 0x01  Bluetooth
Protocol: 0x01  Primary Controller
```

Development identity is:

```text
VID:          0xCAFE
PID:          0x4011
Manufacturer: ESP32 Radio Dongle
Product:      ESP32 Radio Dongle V1
Serial:       12 uppercase hexadecimal digits derived from S3 factory base MAC
```

`0xCAFE:0x4011` is development-only. Distributed production firmware SHALL use a USB VID/PID that the project is authorized to use.

### 8.3 USB Bluetooth configuration

V1 uses the legacy Bluetooth Controller **two-interface** configuration.

#### Interface 0, alternate setting 0 — active HCI transport

- Class/subclass/protocol: E0/01/01.
- HCI commands: host-to-device class control transfers on endpoint 0.
- HCI events: interrupt IN endpoint `0x81`, 16-byte full-speed max packet.
- ACL host-to-controller: bulk OUT endpoint `0x02`, 64-byte full-speed max packet.
- ACL controller-to-host: bulk IN endpoint `0x82`, 64-byte full-speed max packet.

#### Interface 1, alternate setting 0 — zero voice bandwidth

- Class/subclass/protocol: E0/01/01.
- Zero endpoints.
- Represents zero active voice channels.
- V1 provides no nonzero-bandwidth alternate settings and no usable isochronous SCO endpoints.

The total V1 configuration descriptor is 48 bytes and reports `bNumInterfaces=2`.

### 8.4 USB class implementation

V1 uses a project-owned TinyUSB application class shim (`radio_usb_bth`) rather than enabling the pinned stock TinyUSB BTH class.

The shim SHALL:

- service HCI command class-control transfers;
- accept legacy device-targeted host-to-device class-request forms required for single-function compatibility without depending on historical `bRequest`, `wValue`, or `wIndex` values;
- keep interface-targeted command routing strict to the primary interface;
- forward HCI events over interrupt IN;
- reassemble and validate ACL OUT traffic;
- validate and send ACL IN traffic, including full-size bulk-transfer ZLP termination where required;
- claim only the endpoint-free interface 1 alternate setting 0 for the SCO bandwidth interface;
- reject unsupported endpoint-bearing/nonzero-bandwidth SCO forms; and
- report protocol-integrity failures to the bridge recovery path.

### 8.5 Bridge behavior

The S3 bridge SHALL:

- forward host HCI commands to the WROOM;
- forward WROOM HCI events to the host;
- forward ACL data bidirectionally;
- preserve HCI packet boundaries and ordering within required streams;
- use bounded queues;
- honor UART CTS/RTS;
- reject unexpected SCO packets rather than silently exposing pseudo-support; and
- expose development counters for transport/lifecycle errors without putting diagnostic bytes on USB HCI endpoints.

### 8.6 USB lifecycle

The S3 source SHALL handle USB reset/configuration/suspend/resume/disconnect transitions and controlled recovery.

Physical acceptance must still verify:

- native-USB enumeration;
- repeated plug/replug;
- real host reset and suspend/resume behavior; and
- recovery after downstream controller loss.

### 8.7 Debugging separation

Debug logging SHALL NOT corrupt HCI UART traffic or USB Bluetooth endpoints.

Release firmware uses the release configuration documented in `docs/V1_RELEASE_CONFIGURATION.md`; hardware logging-load impact remains a physical performance test.

---

## 9. Host Behavior Requirements

### 9.1 Windows

On a supported Windows installation with normal Bluetooth components present:

1. Plugging in the S3 native USB connection SHALL enumerate a USB Bluetooth controller.
2. Windows SHALL bind normal in-box USB Bluetooth support without a project-specific driver package/INF.
3. Bluetooth SHALL appear in the normal Windows UI/device stack.
4. Normal discovery, pairing, connect, disconnect, and reconnect operations SHALL work.
5. No ESP32 Radio Dongle host application, daemon, serial helper, or custom driver SHALL be required.

### 9.2 Linux

On a mainstream Linux installation with normal kernel USB Bluetooth support and BlueZ present:

1. Plugging in the S3 native USB connection SHALL bind through the standard USB Bluetooth path.
2. An HCI controller SHALL become available to BlueZ.
3. Normal discovery, pairing, connect, disconnect, and reconnect operations SHALL work.
4. `btattach`, a custom serial helper, project kernel module, or ESP32 Radio Dongle daemon SHALL NOT be required.

### 9.3 Driverless definition

For V1, "driverless" means **no project-specific host driver or software installation is required for normal Bluetooth operation**. The operating system's ordinary Bluetooth stack and USB Bluetooth transport driver remain expected dependencies.

---

## 10. Bluetooth Functional Scope

### 10.1 BLE

V1 acceptance SHALL include:

- discovery/scanning through the host stack;
- pairing/bonding where applicable;
- representative GATT connection/use;
- disconnect/reconnect; and
- repeated scan/connect cycles.

### 10.2 Bluetooth Classic over ACL

V1 acceptance SHALL include:

- Classic inquiry/discovery;
- pairing;
- connection to representative Classic devices;
- representative HID operation; and
- reconnect after link loss or USB reconnect.

A2DP or another sustained ACL workload SHOULD be used to exercise the bridge under higher traffic.

### 10.3 SCO/eSCO decision

SCO/eSCO synchronous voice transport is intentionally **out of V1 scope**.

V1 SHALL:

- configure zero synchronous BR/EDR controller connections;
- expose interface 1 alternate setting 0 with zero endpoints for zero voice bandwidth;
- expose no nonzero-bandwidth SCO alternate settings;
- expose no usable USB voice isochronous endpoints; and
- not claim HFP/HSP voice-audio support.

This is a documented V1 product limitation, not an unresolved decision.

---

## 11. Reliability and Failure Handling

### 11.1 Required failure classes

The implementation SHALL detect or safely tolerate:

- malformed/invalid H4 packet type;
- truncated HCI packet;
- invalid/oversized HCI packet length;
- UART framing/overflow/parity/driver errors;
- queue exhaustion;
- WROOM controller failure/reset;
- S3 USB reset/disconnect;
- unexpected controller silence/timeouts during initialization;
- unsupported SCO traffic; and
- repeated reconnect cycles.

### 11.2 Fail-closed behavior

On unrecoverable transport corruption, firmware SHALL reset/reinitialize rather than forward ambiguous bytes.

No error path may fabricate successful HCI completion for an operation the real controller did not complete.

### 11.3 Recovery

The current fatal-integrity recovery policy is deterministic MCU restart followed by normal initialization/readiness probing.

Source/CI evidence proves the policy exists; physical acceptance must prove its behavior under real reset, cable, and host timing.

---

## 12. Performance Requirements

V1 prioritizes correctness and stability over maximum throughput.

Performance acceptance SHALL include:

- no bridge-caused malformed HCI framing under sustained ACL traffic;
- stable hardware RTS/CTS operation;
- no unbounded queue growth;
- observable queue high-water/full counters;
- no USB/UART starvation caused by logging/control work; and
- enough sustained throughput for representative Classic and BLE workloads.

The project SHALL measure actual throughput, UART errors, and queue high-water marks before V1 final acceptance.

The final UART baud is a measured hardware decision, not a nominal-rate assumption.

---

## 13. Build, Component, and Release Structure

V1 uses ESP-IDF **v5.5.5 exactly** for both production targets.

Production projects:

```text
firmware/esp32_wroom_bt_controller/
firmware/esp32s3_usb_bridge/
```

Shared production transport component:

```text
firmware/components/radio_h4/
```

Development-only hardware-smoke projects/components:

```text
firmware/bringup/esp32_wroom_uart_smoke/
firmware/bringup/esp32s3_uart_smoke/
firmware/components/radio_uart_smoke/
```

Requirements:

- Production projects SHALL remain independently buildable/flashable.
- Production component discovery SHALL NOT pull in `radio_uart_smoke`.
- CI SHALL enforce production/development component separation.
- Release builds SHALL use `sdkconfig.release` and the WARN-only/reproducible-build policy in `docs/V1_RELEASE_CONFIGURATION.md`.
- Development smoke images SHALL never substitute for a production image during final acceptance.

---

## 14. Test and Evidence Strategy

Testing is layered so software evidence is never confused with physical interoperability evidence.

### 14.1 Host/software tests

Host-runnable tests cover:

- H4 parser/packet validation;
- fragmentation/back-to-back frames;
- queue limits/high-water/full behavior;
- production USB class control/event/ACL behavior;
- legacy HCI control-request compatibility;
- USB ACL reassembly and transfer termination;
- exact production USB descriptor bytes/strings; and
- release/component policy checks.

### 14.2 Firmware build validation

CI SHALL build:

- WROOM production firmware;
- S3 production firmware;
- both dedicated UART smoke images; and
- both WARN-only production release profiles.

### 14.3 Hardware bring-up tests

Physical acceptance must verify:

- common ground and correct wiring;
- bidirectional UART;
- RTS/CTS assertion/deassertion and real backpressure;
- reset/boot pin behavior;
- real WROOM HCI command/event exchange; and
- native USB enumeration.

### 14.4 Linux acceptance

At minimum:

- cold plug and warm replug;
- boot with dongle attached;
- normal kernel USB Bluetooth/BlueZ controller discovery;
- no project helper/driver;
- BLE scan/pair/connect/use;
- Classic scan/pair/connect;
- HID test;
- sustained ACL traffic; and
- repeated reconnect cycles.

### 14.5 Windows acceptance

At minimum:

- cold plug and warm replug;
- boot with dongle attached;
- automatic binding to normal in-box Bluetooth support;
- no project-specific driver/INF;
- BLE scan/pair/connect/use;
- Classic scan/pair/connect;
- HID test;
- sustained ACL traffic; and
- repeated reconnect cycles.

### 14.6 Recovery/stability

Physical stress acceptance includes:

- repeated USB cycles;
- host reboot/suspend-resume;
- WROOM reset while S3 remains powered;
- S3 reset while WROOM remains powered;
- queue/traffic pressure;
- final UART-rate characterization; and
- multi-hour stability runs.

---

## 15. Evidence Policy

A software implementation, host test, static review, or CI build SHALL NOT by itself close a task whose acceptance criterion requires physical boards or a real host Bluetooth stack.

The current evidence mapping is maintained in:

- `docs/V1_EVIDENCE_INDEX.md`;
- `docs/V1_DOCUMENTATION_EVIDENCE_AUDIT.md`; and
- `docs/ESP32_RADIO_DONGLE_V1_TODO.md`.

Device tests may be deferred without being treated as PASS or FAIL.

---

## 16. V1 Acceptance Criteria

V1 is complete only when all of the following are true:

1. Both production firmware images build reproducibly from the repository.
2. The selected/reference wiring works without undocumented extra circuitry.
3. Physical UART and RTS/CTS behavior pass.
4. The WROOM exposes working BR/EDR + BLE controller functionality to the S3.
5. The S3 enumerates through native USB as the documented two-interface USB Bluetooth controller.
6. Windows recognizes/uses the adapter without project-specific host software or driver.
7. Linux recognizes/uses the adapter without `btattach`, custom driver, daemon, or helper.
8. Representative BLE discovery/pair/connect/use operations pass on both operating systems.
9. Representative Bluetooth Classic discovery/pair/connect operations pass on both operating systems.
10. A representative Bluetooth HID device works on both operating systems.
11. Sustained ACL traffic remains stable without bridge-caused HCI corruption.
12. USB unplug/replug and host reboot recovery work without reflashing.
13. Required reset/suspend-resume/recovery scenarios pass.
14. Final UART rate and stability evidence are recorded.
15. Transport/controller failures fail closed and recover predictably.
16. No V1 blocker is hidden behind development-only host software or diagnostic firmware.
17. Release configuration and artifact hashes are captured from the exact hardware-qualified release commit.
18. Documentation accurately reflects the hardware-tested behavior and known limitations.

---

## 17. V2 Architectural Reservation: Wi-Fi

Wi-Fi is not part of V1 implementation or acceptance.

V2 may add USB Wi-Fi functionality using the ESP32-S3 radio while retaining V1 Bluetooth functionality through the WROOM.

One candidate experiment is an RTL8188EU-compatible USB facade so an existing host Wi-Fi driver may bind. This is research, not a V1 dependency or promise.

V2 work must preserve:

- the WROOM-S3 HCI UART contract unless a deliberate hardware revision is approved;
- normal driverless V1 Bluetooth behavior;
- USB resource headroom where practical; and
- explicit separation between V1 acceptance and experimental Wi-Fi compatibility.

---

## 18. Decisions Locked for V1

The following are decided unless deliberately revised with SPEC/TODO/evidence updates:

- Two-MCU design: ESP32-S3 + original ESP32-WROOM-32.
- Selected initial boards are recorded in `V1_BOARD_VERIFICATION.md`.
- WROOM supplies Bluetooth Classic + BLE controller/radio functionality.
- S3 supplies native USB Bluetooth HCI transport.
- ESP-IDF v5.5.5 is the exact supported baseline.
- HCI H4 is the inter-MCU protocol.
- Hardware RTS/CTS is part of the reference transport.
- Shared initial UART rate is 115200 baud; final rate requires hardware measurement.
- Reference pins are S3 GPIO4/5/6/7 and WROOM GPIO16/17/25/26.
- Host-facing V1 is standard USB Bluetooth, not USB serial Bluetooth.
- USB Bluetooth uses the two-interface legacy Controller layout described in Section 8.
- The S3 uses the project-owned `radio_usb_bth` TinyUSB application class shim.
- Development USB identity is `0xCAFE:0x4011`; production authorization is still required before distribution.
- SCO/eSCO synchronous voice transport is out of V1 scope.
- No project-specific Windows/Linux host software is permitted for normal V1 operation.
- Development smoke firmware is not a production dependency.
- Wi-Fi is V2.

---

## 19. Decisions Still Deferred to Hardware/Release Evidence

The following remain intentionally unresolved until the specified evidence exists:

- final UART baud rate;
- measured throughput/stability limits;
- final tested Windows versions/builds;
- final tested Linux distribution/kernel/BlueZ versions;
- final BLE/Classic peripheral compatibility matrix;
- physical recovery behavior/timing;
- whether a dedicated inter-MCU reset/control GPIO is actually necessary for a later hardware revision;
- final custom-PCB/power implementation; and
- production USB VID/PID authorization/assignment.

Any future decision that affects host compatibility or the physical contract must be reflected in this SPEC, the TODO, and the evidence index before V1 release.
