# V1 Security, Identity, and Failure-Semantics Review

This review records software-only evidence for V1-1101 through V1-1103. Hardware and release-configuration evidence remains separate.

## USB identity review

Development USB identity is defined in `firmware/esp32s3_usb_bridge/main/usb_descriptors.c` and documented in `docs/USB_BLUETOOTH_V1.md`.

Current development policy:

- manufacturer: `ESP32 Radio Dongle`;
- product: `ESP32 Radio Dongle V1`;
- development VID/PID: `0xCAFE:0x4011`;
- serial number: 12 uppercase hexadecimal digits derived from the ESP32-S3 factory base MAC; and
- production/distributed builds must use a VID/PID the project is authorized to use.

The placeholder development identity is not a claim to another manufacturer's identity and is not approved as a production VID/PID by this project.

## H4 input bounds

The shared H4 implementation in `firmware/components/radio_h4/` has explicit per-type limits and one finite maximum packet buffer.

Validation properties:

- the H4 packet type is validated before type-specific header interpretation;
- command, event, ACL, and SCO lengths are parsed only after the complete type-specific header is present;
- impossible/unsupported packet types fail closed;
- declared payloads above the configured limit fail closed;
- complete-packet validation rejects trailing bytes and truncation;
- streaming parser corruption enters a failed state until explicit reset; and
- the steady-state parser does not allocate based on an untrusted HCI length.

`tests/host/test_radio_h4.c` exercises valid command/event/ACL/SCO model packets, fragmentation, back-to-back delivery, invalid types, oversized ACL length, truncation, trailing bytes, and queue exhaustion.

CI compiles this host test with `-Wall -Wextra -Werror -pedantic` and executes it on every push/PR.

## Queue bounds

The shared queue model has a fixed compile-time capacity. The actual S3 and WROOM bridges likewise use fixed-capacity FreeRTOS queues whose items are fixed-size `radio_h4_packet_t` values.

Queue exhaustion is counted and treated as a fatal bridge-integrity condition; the firmware does not intentionally discard an oldest/newest HCI packet and continue pretending framing/state is intact.

The parser/queue host regression covers queue fill, full rejection, high-water tracking, and drain-to-empty behavior.

## USB transfer bounds

The project-owned S3 USB Bluetooth class uses statically sized buffers for:

- HCI commands;
- HCI events;
- ACL OUT reassembly; and
- ACL IN transmission.

The class rejects control requests larger than the command buffer. ACL OUT accumulation checks both individual full-speed USB packet size and aggregate HCI ACL buffer capacity before copying. The HCI ACL header's declared length must match the completed packet before forwarding. Event and ACL IN submission validates the complete payload length before copying to the static transmit buffer.

There is no allocation whose size is taken directly from a host-provided HCI/USB length.

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

## Items not closed by this review

This software review does not prove:

- electrical RTS/CTS behavior;
- real USB enumeration or OS driver binding;
- physical WROOM reset detection/recovery under every reset timing;
- pairing/security behavior of real peripherals;
- release logging volume/content; or
- production USB identity authorization.

Those remain acceptance/release tasks in `docs/ESP32_RADIO_DONGLE_V1_TODO.md`.
