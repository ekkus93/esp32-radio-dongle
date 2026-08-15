# V1 Security, Identity, and Failure-Semantics Review

This review records software evidence for V1-1101 through V1-1103 and links the completed V1-1104 release-configuration review. Physical interoperability/timing and production USB identity authorization remain separate acceptance/release concerns.

## USB identity review

Development USB identity is defined in `firmware/esp32s3_usb_bridge/main/usb_descriptors.c` and documented in `docs/USB_BLUETOOTH_V1.md`.

Current development policy:

- manufacturer: `ESP32 Radio Dongle`;
- product: `ESP32 Radio Dongle V1`;
- development VID/PID: `0xCAFE:0x4011`;
- serial number: 12 uppercase hexadecimal digits derived from the ESP32-S3 factory base MAC; and
- production/distributed builds must use a VID/PID the project is authorized to use.

The placeholder development identity is not production USB identity authorization.

## H4 input bounds

The shared H4 implementation in `firmware/components/radio_h4/` has explicit per-type limits and one finite maximum packet buffer.

Validation properties:

- the H4 packet type is validated before type-specific header interpretation;
- command, event, ACL, and SCO model lengths are parsed only after the complete type-specific header is present;
- impossible/unsupported packet types fail closed;
- declared payloads above the configured limit fail closed;
- complete-packet validation rejects trailing bytes and truncation;
- streaming parser corruption enters a failed state until explicit reset; and
- the steady-state parser does not allocate based on an untrusted HCI length.

`tests/host/test_radio_h4.c` exercises valid command/event/ACL/SCO model packets, fragmentation, back-to-back delivery, invalid types, oversized ACL length, truncation, trailing bytes, and queue exhaustion.

CI compiles this host test with `-Wall -Wextra -Werror -pedantic` and executes it on firmware/test workflow runs.

## Queue bounds

The shared queue model has a fixed compile-time capacity. The S3 and WROOM bridges use fixed-capacity FreeRTOS queues whose items are fixed-size `radio_h4_packet_t` values.

Queue exhaustion is counted and treated as a fatal bridge-integrity condition; firmware does not intentionally discard an oldest/newest HCI packet and continue pretending framing/state is intact.

The parser/queue host regression covers queue fill, full rejection, high-water tracking, and drain-to-empty behavior.

## USB transfer bounds

The project-owned S3 USB Bluetooth class uses statically sized buffers for:

- HCI commands;
- HCI events;
- ACL OUT reassembly; and
- ACL IN transmission.

The class rejects control requests larger than the command buffer. ACL OUT accumulation checks both individual full-speed USB packet size and aggregate HCI ACL buffer capacity before copying. The HCI ACL header's declared length must match the completed packet before forwarding. Event and ACL IN submission validates the complete payload length before copying to the static transmit buffer.

There is no allocation whose size is taken directly from a host-provided HCI/USB length.

## HCI control-request compatibility and routing

The project-owned class accepts legacy single-function device-targeted host-to-device class requests as HCI commands without relying on historical `bRequest`, `wValue`, or `wIndex` values.

Interface-targeted command requests remain strict to the primary Bluetooth interface and recommended request/value form so a future composite expansion cannot casually route a command to the wrong interface.

Device-to-host, non-class, misrouted interface, and oversized requests are rejected by the HCI command path.

`tests/host/test_radio_usb_bth_control_compat.c` exercises these compatibility and rejection cases.

## Controller-originated validation

The WROOM VHCI receive callback validates complete H4 packets before queueing them toward the S3. The S3 streaming parser independently validates the WROOM UART stream before routing event or ACL payloads to USB.

Unexpected SCO traffic in the V1 no-SCO configuration is treated as an integrity/configuration failure rather than silently forwarded or discarded mid-session.

## Failure semantics

V1 deliberately avoids optimistic/fabricated behavior:

- the S3 probes the real WROOM with HCI Reset and Read Local Version Information before installing/attaching normal USB Bluetooth service;
- no synthetic successful HCI response is generated when the WROOM is unavailable;
- malformed H4 data is not silently skipped in search of a later apparent packet boundary;
- queue exhaustion is not silently converted into packet loss;
- UART framing/overflow errors cause controlled recovery; and
- USB Bluetooth class protocol errors cause controlled recovery.

The current recovery policy for a fatal transport-integrity failure is whole-MCU restart. S3 restart re-runs the WROOM HCI readiness probe before normal USB Bluetooth service is exposed again. WROOM restart reinitializes its Bluetooth controller and H4 transport.

Development diagnostics record the reason category and counters needed to distinguish malformed traffic, queue pressure, UART errors, USB lifecycle/protocol events, unexpected SCO traffic, and recovery attempts.

## Release logging/content review

V1-1104 is complete for software configuration and static content review. `docs/V1_RELEASE_CONFIGURATION.md` records the release policy.

The production firmware was reviewed for raw/pairing-sensitive logging and currently does not intentionally emit:

- HCI command/event/ACL payload dumps;
- Bluetooth link keys;
- PIN/passkey material;
- LTK/IRK/CSRK values; or
- arbitrary packet hex dumps.

`scripts/check-release-logging.sh` rejects ESP-IDF buffer/hex-dump logging APIs in the production firmware trees. Release configuration compiles application/bootloader logging at WARN-or-lower policy as documented by the release review.

This static review does not prove that development logging has no timing impact under real sustained Bluetooth traffic. That remains V1-904.

## Development/production boundary

The production projects import the shared `radio_h4` component explicitly and do not import the bring-up-only `radio_uart_smoke` component. `scripts/check-component-boundaries.sh` enforces the boundary in CI.

The dedicated UART smoke images are hardware acceptance tools only and are never a substitute for the two production images.

## Items not closed by this review

This software review does not prove:

- electrical RTS/CTS behavior;
- real USB enumeration or OS driver binding;
- physical WROOM/S3 reset behavior under real timing;
- pairing/security/interoperability behavior of real peripherals;
- development-log timing impact under sustained hardware traffic; or
- production USB identity authorization.

Those remain acceptance/release tasks in `docs/ESP32_RADIO_DONGLE_V1_TODO.md`.
