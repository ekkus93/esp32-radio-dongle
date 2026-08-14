# ESP32 Radio Dongle V1 Specification

## 1. Purpose

ESP32 Radio Dongle V1 is a dual-MCU USB Bluetooth adapter built from:

- an **ESP32-S3** acting as the native USB device and USB-to-HCI bridge; and
- an **ESP32-WROOM-32** acting as the Bluetooth controller/radio for Bluetooth Classic (BR/EDR) and Bluetooth Low Energy (BLE).

The defining V1 product requirement is:

> After both firmware images are flashed and the hardware is connected, plugging the ESP32-S3 native USB port into a normal Windows or Linux computer must make the device appear as an ordinary USB Bluetooth adapter without installing project-specific host software, drivers, daemons, scripts, or helper applications.

Windows and Linux must use their normal built-in Bluetooth stacks and USB Bluetooth support.

Wi-Fi functionality is explicitly deferred to V2.

---

## 2. V1 Goals

V1 SHALL:

1. Provide Bluetooth Classic (BR/EDR) and BLE through the ESP32-WROOM-32 radio.
2. Present a standards-compliant USB Bluetooth HCI interface through the ESP32-S3 native USB peripheral.
3. Work with the normal Windows Bluetooth stack without a project-specific Windows driver.
4. Work with the normal Linux Bluetooth stack (`btusb`/BlueZ) without a project-specific Linux driver or serial-HCI attach utility.
5. Use a direct HCI transport between the two MCUs rather than running Bluetooth profiles on the ESP32-S3.
6. Use UART H4 with hardware RTS/CTS flow control between the ESP32-S3 and ESP32-WROOM-32.
7. Keep the ESP32-WROOM-32 UART0 path available for development flashing/logging.
8. Recover cleanly from USB disconnect/reconnect, host reboot, controller reset, and transient transport failure.
9. Keep the architecture compatible with a later V2 that adds Wi-Fi through the ESP32-S3 without redesigning the V1 inter-MCU link.

---

## 3. V1 Non-Goals

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

V1 may include development-only diagnostics, scripts, test tools, and logging. Those tools must not be required for normal end-user Bluetooth operation.

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

The Windows or Linux host owns the Bluetooth host stack and profiles. Examples include pairing, HID, A2DP, GATT, and other host-side profile behavior.

### 4.2 ESP32-S3 responsibility

The ESP32-S3 owns:

- native USB enumeration;
- USB Bluetooth HCI transport;
- HCI packet forwarding;
- UART flow control and buffering;
- controller supervision/recovery;
- bridge diagnostics; and
- future V2 USB composite expansion.

### 4.3 ESP32-WROOM-32 responsibility

The ESP32-WROOM-32 owns:

- Bluetooth controller initialization;
- BR/EDR radio/controller operation;
- BLE radio/controller operation;
- standard HCI controller behavior; and
- HCI H4 transport to the ESP32-S3.

The WROOM-32 SHALL NOT require the ESP32-S3 to interpret normal Bluetooth profile semantics.

---

## 5. Hardware

### 5.1 V1 prototype hardware

Minimum development hardware:

- 1 x ESP32-S3 development board exposing the chip's native USB device connection.
- 1 x ESP32-WROOM-32 development board.
- Jumper wires for HCI UART and common ground.
- USB cable for the ESP32-S3 native USB connection.
- A second USB cable may be used during development to flash/debug the WROOM-32.

### 5.2 Provisional final pin assignment

This pin assignment is considered the V1 reference assignment unless changed deliberately later:

| Signal | ESP32-S3 | Direction | ESP32-WROOM-32 |
|---|---:|:---:|---:|
| HCI TX | GPIO4 | -> | GPIO16 / RX2 |
| HCI RX | GPIO5 | <- | GPIO17 / TX2 |
| HCI RTS | GPIO6 | -> | GPIO25 / CTS |
| HCI CTS | GPIO7 | <- | GPIO26 / RTS |
| Ground | GND | <-> | GND |

TX/RX and RTS/CTS are crossed between endpoints.

### 5.3 Electrical requirements

- Both MCUs use 3.3 V logic; no level shifter is expected for the UART signals.
- During prototype development, the two dev boards may be independently USB-powered while sharing ground.
- The 3.3 V regulator outputs of two independently powered development boards SHALL NOT be tied together unless the particular hardware design explicitly supports it.
- A future integrated PCB should use one appropriately sized power tree from the USB input.
- The final hardware design must account for simultaneous 2.4 GHz activity if V2 Wi-Fi is later enabled.

### 5.4 USB connector requirement

The host-facing connector must be wired to the **ESP32-S3 native USB peripheral**, not merely to a USB-to-UART bridge.

---

## 6. Inter-MCU HCI Transport

### 6.1 Transport format

The inter-MCU transport SHALL use Bluetooth HCI UART Transport Layer H4 framing.

The bridge SHALL preserve HCI packet boundaries and packet types, including at minimum:

- HCI Command packets;
- HCI Event packets;
- ACL data packets; and
- SCO/eSCO data packets where supported by the selected controller/USB transport implementation.

### 6.2 UART configuration

- Hardware UART SHALL be used on both processors.
- RTS/CTS hardware flow control SHALL be enabled for the production V1 data path.
- The UART baud rate SHALL be configurable at build time and/or by a single shared configuration definition.
- Initial bring-up may use a conservative baud rate.
- The final V1 baud rate SHALL be selected only after sustained bidirectional stress testing.
- Firmware SHALL NOT silently continue with mismatched transport settings.

### 6.3 Buffering

The bridge SHALL use bounded buffers.

Requirements:

- No unbounded allocation in the steady-state HCI forwarding path.
- USB-to-UART and UART-to-USB paths must have independent buffering.
- Flow control/backpressure must be applied before buffer exhaustion.
- Buffer exhaustion must be observable through diagnostics and must not corrupt packet framing.
- HCI packets must never be partially discarded and then passed onward as if valid.

### 6.4 Synchronization and startup

The ESP32-S3 SHALL not advertise the Bluetooth controller as operational until the WROOM-32 controller path has reached the required initialization state.

The startup sequence SHALL have explicit states, for example:

1. S3 boot.
2. UART initialization.
3. WROOM controller availability/reset handling.
4. HCI transport ready.
5. USB Bluetooth interface ready.
6. Normal forwarding.

Exact implementation may vary, but readiness must be deterministic and diagnosable.

---

## 7. ESP32-WROOM-32 Firmware

### 7.1 Role

The WROOM firmware SHALL run the ESP32 Bluetooth subsystem in controller-oriented mode suitable for an external host stack.

### 7.2 Required capabilities

The firmware SHALL:

- initialize dual-mode Bluetooth support for BR/EDR and BLE;
- expose the controller through the HCI transport used by the S3;
- use the reference UART pins in Section 5.2;
- enable UART hardware flow control;
- keep UART0 available for development logging/flashing where practical;
- avoid application-level Bluetooth profiles in V1;
- expose deterministic boot and failure diagnostics during development;
- detect fatal controller initialization failure; and
- enter a recoverable failure state rather than presenting malformed HCI traffic.

### 7.3 Reset/recovery

The WROOM firmware SHALL tolerate:

- S3 restart while the WROOM remains powered;
- host USB unplug/replug while the WROOM remains powered;
- WROOM restart while the S3 remains powered; and
- repeated controller initialization across normal development cycles.

If a dedicated cross-MCU reset/control signal is later proven necessary, it may be added as an explicitly documented hardware revision. It is not part of the current reference pinout.

---

## 8. ESP32-S3 Firmware

### 8.1 Role

The ESP32-S3 SHALL present the entire device to the host as a normal USB Bluetooth controller while forwarding HCI traffic to the WROOM-32.

### 8.2 USB device requirements

The V1 USB interface SHALL identify as the standard USB Bluetooth wireless-controller class expected by normal host Bluetooth drivers.

The descriptor set SHALL use the appropriate Bluetooth device/interface class, subclass, and protocol values and the endpoint types required by the USB Bluetooth transport.

Expected transport roles include:

- control transfers for HCI commands;
- interrupt IN for HCI events;
- bulk OUT/IN for ACL traffic; and
- isochronous endpoints where required and supported for SCO/eSCO transport.

Descriptor correctness SHALL be validated using host-side USB inspection tools during development.

### 8.3 USB identity

Development firmware SHALL define project-owned development values for:

- manufacturer string;
- product string;
- serial number strategy; and
- development VID/PID policy.

The project SHALL NOT claim another manufacturer's VID/PID for production use.

A proper VID/PID strategy is a release/commercialization concern and SHALL NOT block early functional development.

### 8.4 Bridge behavior

The S3 bridge SHALL:

- forward host HCI commands to the WROOM;
- forward WROOM HCI events to the host;
- forward ACL data bidirectionally;
- support SCO/eSCO forwarding if the selected USB/controller implementation exposes it;
- preserve packet ordering within each required transport stream;
- never reinterpret profile-level traffic unnecessarily;
- use bounded queues;
- honor UART CTS/RTS; and
- expose counters for transport errors during development.

### 8.5 USB lifecycle

The S3 firmware SHALL correctly handle:

- USB reset;
- enumeration;
- configuration;
- suspend;
- resume;
- unplug/replug;
- host reboot with dongle attached; and
- repeated enumeration without requiring reflashing or physical reset.

### 8.6 Debugging separation

Debug logging SHALL NOT corrupt the HCI UART transport or the USB Bluetooth interface.

Logs should use a separate development channel where practical. Logging volume must be bounded so that debug output cannot starve HCI processing.

---

## 9. Host Behavior Requirements

### 9.1 Windows

On a supported Windows installation with its normal Bluetooth components present:

1. Plugging in the device SHALL enumerate a USB Bluetooth adapter.
2. Windows SHALL bind its normal in-box USB Bluetooth support without a project-specific driver package.
3. Bluetooth SHALL appear in the normal Windows UI/device stack.
4. Normal discovery, pairing, connect, disconnect, and reconnect operations SHALL work.
5. No ESP32 Radio Dongle host application SHALL be required.

### 9.2 Linux

On a mainstream Linux installation with normal kernel USB Bluetooth support and BlueZ present:

1. Plugging in the device SHALL bind through the standard USB Bluetooth path.
2. An HCI controller SHALL become available to BlueZ.
3. Normal discovery, pairing, connect, disconnect, and reconnect operations SHALL work.
4. `btattach`, a custom serial helper, or an ESP32 Radio Dongle daemon SHALL NOT be required.

### 9.3 Driverless definition

For V1, "driverless" means **no project-specific host driver or software installation is required**. The operating system's ordinary Bluetooth stack and class/transport drivers are expected dependencies.

---

## 10. Bluetooth Functional Scope

V1 functional validation SHALL cover both controller modes:

### 10.1 BLE

At minimum:

- passive/active discovery as exercised by the host;
- pairing/bonding where applicable;
- GATT connection to a representative BLE peripheral;
- disconnect/reconnect; and
- repeated scan/connect cycles.

### 10.2 Bluetooth Classic

At minimum:

- Classic inquiry/discovery;
- pairing;
- connection to representative Classic devices;
- HID device operation; and
- reconnect after link loss or USB reconnect.

A2DP or another high-throughput Classic profile SHOULD be included in acceptance testing because it exercises ACL traffic heavily.

SCO/eSCO voice support SHALL be treated separately: the architecture must not accidentally preclude it, but final V1 acceptance may mark it optional if the ESP32 controller/USB stack combination cannot support it reliably without disproportionate scope expansion. Any such limitation must be documented explicitly rather than silently omitted.

---

## 11. Reliability and Failure Handling

### 11.1 Required failure classes

The implementation SHALL detect or safely tolerate:

- malformed/invalid H4 packet type;
- truncated HCI packet;
- invalid HCI packet length;
- UART framing/overflow errors;
- queue exhaustion;
- WROOM controller failure/reset;
- S3 USB reset;
- host disconnect;
- unexpected controller silence/timeouts during initialization; and
- repeated rapid reconnect cycles.

### 11.2 Fail-safe behavior

On unrecoverable transport corruption, the firmware SHALL reset/reinitialize the affected transport/controller rather than forwarding ambiguous bytes.

No error path may intentionally fabricate successful HCI completion for an operation that was not completed by the controller.

### 11.3 Watchdogs

Watchdogs may be used, but watchdog resets must be attributable through retained/reset-reason diagnostics during development.

---

## 12. Performance Requirements

V1 prioritizes correctness and stability over maximum throughput.

Performance acceptance SHALL include:

- no avoidable HCI packet loss under sustained ACL traffic;
- stable operation with hardware flow control enabled;
- no unbounded queue growth;
- no USB starvation caused by logging or control tasks;
- no UART starvation caused by USB handling; and
- enough sustained throughput for representative Classic and BLE workloads.

The project SHALL measure actual throughput and queue high-water marks before declaring V1 complete.

---

## 13. Development and Build Structure

The repository SHOULD keep the two firmware targets separate while sharing protocol/configuration definitions where useful.

Recommended structure:

```text
esp32-radio-dongle/
  docs/
    ESP32_RADIO_DONGLE_V1_SPEC.md
    ESP32_RADIO_DONGLE_V1_TODO.md
  firmware/
    esp32s3_usb_bridge/
    esp32_wroom_bt_controller/
  components/
    shared_protocol/        # optional shared definitions/tests
  scripts/
  tests/
```

Each firmware target SHALL be independently buildable and flashable.

The project SHOULD pin or document the supported ESP-IDF version rather than depending indefinitely on whatever version happens to be installed on a developer machine.

---

## 14. Test Strategy

Testing SHALL be layered.

### 14.1 Host-independent tests

Where practical, test:

- H4 parser/encoder behavior;
- packet boundary handling;
- queue behavior;
- malformed frame rejection;
- buffer limits;
- flow-control state transitions;
- reset state machine; and
- USB/HCI mapping logic.

### 14.2 Hardware bring-up tests

Verify:

- exact pin wiring;
- UART bidirectional traffic;
- RTS/CTS operation;
- WROOM controller initialization;
- basic HCI command/event exchange; and
- stable USB enumeration.

### 14.3 Linux acceptance tests

At minimum:

- cold plug;
- warm replug;
- boot with dongle attached;
- controller discovery by the kernel/BlueZ;
- BLE scan/pair/connect;
- Classic scan/pair/connect;
- HID device test;
- sustained traffic test; and
- repeated disconnect/reconnect cycles.

### 14.4 Windows acceptance tests

At minimum:

- cold plug;
- warm replug;
- boot with dongle attached;
- automatic binding to the normal Windows Bluetooth stack;
- no project-specific driver installation;
- BLE scan/pair/connect;
- Classic scan/pair/connect;
- HID device test;
- sustained traffic test; and
- repeated disconnect/reconnect cycles.

### 14.5 Stress tests

Include:

- repeated USB cycles;
- repeated Bluetooth connection cycles;
- simultaneous command/event and ACL load;
- queue pressure;
- controller reset during host activity;
- S3 reset during controller activity; and
- multi-hour stability runs before release.

---

## 15. V1 Acceptance Criteria

V1 is complete only when all of the following are true:

1. Both firmware images build reproducibly from the repository.
2. The documented reference wiring works without undocumented extra circuitry.
3. The WROOM-32 exposes working BR/EDR + BLE controller functionality to the S3.
4. The S3 enumerates through native USB as a USB Bluetooth controller.
5. Windows recognizes and uses the adapter without installing project-specific host software or a project-specific driver.
6. Linux recognizes and uses the adapter without `btattach`, a custom driver, daemon, or project-specific helper.
7. Representative BLE discovery/pair/connect operations pass on both operating systems.
8. Representative Bluetooth Classic discovery/pair/connect operations pass on both operating systems.
9. A representative Bluetooth HID device works on both operating systems.
10. Sustained ACL traffic is stable and does not corrupt HCI framing.
11. USB unplug/replug and host reboot recovery work without reflashing.
12. Transport/controller failures fail closed and recover predictably.
13. No known V1 blocker is hidden behind development-only host software.
14. Documentation accurately describes flashing, wiring, normal use, known limitations, and test evidence.

---

## 16. V2 Architectural Reservation: Wi-Fi

Wi-Fi is not part of V1 implementation or acceptance.

V2 may add USB Wi-Fi functionality using the ESP32-S3's Wi-Fi radio while retaining V1 Bluetooth functionality through the WROOM-32.

One candidate V2 experiment is an RTL8188EU-compatible USB facade so that an existing host Wi-Fi driver may bind to the ESP32-S3. This is an experiment, not a V1 dependency or promise.

V1 SHALL therefore avoid unnecessary assumptions that prevent:

- a future USB composite configuration;
- additional USB interfaces/endpoints if resources permit;
- concurrent S3 Wi-Fi activity; or
- retaining the existing WROOM-S3 HCI UART link unchanged.

V2 design work must not weaken V1's driverless Bluetooth behavior.

---

## 17. Decisions Locked for V1

The following are considered decided unless deliberately revised:

- Two-MCU design: ESP32-S3 + ESP32-WROOM-32.
- WROOM-32 supplies Bluetooth Classic + BLE.
- S3 supplies native USB.
- HCI H4 is the inter-MCU protocol.
- Hardware RTS/CTS is part of the reference transport.
- Reference pins are GPIO4/5/6/7 on S3 and GPIO16/17/25/26 on WROOM-32 as documented above.
- Host-facing V1 is standard USB Bluetooth, not USB serial Bluetooth.
- No project-specific Windows/Linux host software is permitted for normal V1 operation.
- Wi-Fi is V2.

---

## 18. Deferred Decisions

The following may be selected during implementation without changing the architecture:

- exact ESP-IDF release after compatibility validation;
- exact TinyUSB/ESP-IDF USB integration mechanism;
- final UART baud rate;
- exact queue sizes;
- exact development VID/PID values and eventual production USB identity strategy;
- optional dedicated inter-MCU reset/control GPIO;
- final custom PCB power design;
- whether SCO/eSCO voice transport is a V1 release requirement or a documented post-V1 enhancement.

Any deferred decision that affects host compatibility must be recorded in this specification before V1 release.
