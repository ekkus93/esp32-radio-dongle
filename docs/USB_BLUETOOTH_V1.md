# V1 USB Bluetooth Interface

## Host-visible contract

V1 exposes one USB Bluetooth Controller function from the ESP32-S3 native USB peripheral using the legacy Bluetooth USB two-interface layout.

The host-visible class triple is:

```text
Class:    0xE0  Wireless Controller
Subclass: 0x01  Bluetooth
Protocol: 0x01  Bluetooth Programming Interface / Primary Controller
```

The E0/01/01 identity is present in the device descriptor and both Bluetooth interface descriptors.

The configuration contains:

- **Interface 0, alternate setting 0** — HCI command/event/ACL transport with three endpoints.
- **Interface 1, alternate setting 0** — the Bluetooth SCO bandwidth interface in its zero-active-voice state. V1 exposes zero endpoints on this setting and provides no alternate setting with usable SCO endpoints.

The active V1 transport is:

| Traffic | USB transport | Direction |
|---|---|---|
| HCI command | endpoint 0 class control transfer | host -> controller |
| HCI event | interrupt IN `0x81` | controller -> host |
| ACL data | bulk OUT `0x02` | host -> controller |
| ACL data | bulk IN `0x82` | controller -> host |

Full-speed endpoint sizes are 16 bytes for the event interrupt endpoint and 64 bytes for ACL bulk endpoints.

The configuration descriptor is 48 bytes total: the 9-byte configuration descriptor, the 9-byte primary interface, three 7-byte event/ACL endpoint descriptors, and the 9-byte endpoint-free SCO interface alternate setting 0.

Windows and Linux acceptance is based on binding to their normal USB Bluetooth support. V1 shall not require a project-specific INF, kernel module, serial attachment program, daemon, service, or configuration application.

## HCI command control-request compatibility

Bluetooth USB HCI commands are host-to-device class control transfers on endpoint 0.

For a single-function Bluetooth controller, historical hosts may use non-recommended values such as `bRequest=0xE0`. The V1 class shim therefore accepts device-targeted host-to-device class requests as HCI commands without depending on `bRequest`, `wValue`, or `wIndex`.

Interface-targeted requests are deliberately stricter for future composite-device safety: they must use `bRequest=0`, `wValue=0`, and the actual primary interface number. Device-to-host or non-class requests are rejected.

Host regression coverage is in `tests/host/test_radio_usb_bth_control_compat.c`.

## Why V1 owns a small TinyUSB class shim

The pinned TinyUSB implementation contains a Bluetooth HCI class, but its `v0.19.0.3` device driver expects voice/ISO endpoint descriptors while the same implementation labels those voice endpoints as not yet used and provides no SCO/ISO application data API suitable for this project's end-to-end forwarding.

V1 therefore does **not** enable TinyUSB's built-in BTH class. Instead, `radio_usb_bth` is a project-owned TinyUSB application class driver registered through `usbd_app_driver_get_cb()`.

The shim intentionally implements only the data transport V1 can support end to end:

- HCI command control transfers;
- HCI event interrupt IN;
- HCI ACL bulk OUT, including full HCI ACL packet reassembly from USB full-speed packets;
- HCI ACL bulk IN, including zero-length packet termination when a transfer length is an exact endpoint-size multiple;
- the endpoint-free second Bluetooth interface alternate setting 0 for zero active voice bandwidth; and
- protocol-error reporting to the S3 recovery state machine.

This avoids advertising a voice data path that the firmware cannot service while retaining the standard two-interface Bluetooth USB configuration shape.

## SCO/eSCO limitation

V1 does **not** support SCO/eSCO synchronous voice transport.

Consequences:

- interface 1 alternate setting 0 exists but has zero endpoints;
- no usable USB isochronous Bluetooth voice endpoints are advertised;
- no nonzero-bandwidth SCO alternate settings are advertised;
- the WROOM controller is configured with `CONFIG_BTDM_CTRL_BR_EDR_MAX_SYNC_CONN=0`;
- any unexpected H4 SCO packet is treated as a transport/configuration integrity failure; and
- HFP/HSP voice-audio acceptance is out of V1 scope.

This does **not** disable Bluetooth Classic generally. Classic profiles transported over ACL remain in scope, including representative HID and A2DP testing where supported by the host/device under test.

## USB identity

Development descriptors currently use:

```text
VID:          0xCAFE
PID:          0x4011
Manufacturer: ESP32 Radio Dongle
Product:      ESP32 Radio Dongle V1
Serial:       12 uppercase hexadecimal digits derived from the S3 factory base MAC
```

`0xCAFE:0x4011` is a **development-only placeholder identity**. It is not authorization to ship a product under somebody else's assigned USB VID/PID.

A production/release build intended for distribution must use a VID/PID that the project is legally authorized to use. V1 release review must keep this requirement explicit even if development hardware uses the placeholder.

## Readiness sequencing

The S3 does not intentionally attach the USB Bluetooth device before proving that the WROOM controller is reachable.

Startup order is:

1. initialize the H4 UART with RTS/CTS;
2. send HCI Reset to the WROOM controller;
3. require a successful Command Complete event;
4. send HCI Read Local Version Information;
5. require a successful Command Complete response and log controller identity/version fields;
6. start the normal H4 transport tasks; and
7. install/attach the native USB device stack.

This prevents normal startup from presenting a Bluetooth radio to the host when the downstream controller cannot answer HCI commands.

## USB lifecycle and recovery

The S3 tracks USB enumerating, operational, suspended, resumed, and recovering states.

Fatal transport-integrity conditions cause controlled S3 restart instead of allowing ambiguous HCI state to continue. On the next startup, the controller readiness probe sends HCI Reset before USB is attached, re-establishing a clean HCI session.

Development diagnostics include packet counts, UART errors, malformed-packet counts, queue high-water/full counts, USB attach/detach/suspend/resume counts, USB protocol errors, unexpected SCO counts, and recovery counts. Diagnostic logging is not carried over the USB Bluetooth HCI endpoints.

## Software-only validation

Host tests compile and execute the actual production USB class and descriptor source with strict warnings enabled. They verify:

- E0/01/01 device/interface identity;
- two-interface, 48-byte configuration layout;
- interface 0 event/ACL endpoint addresses, types, and sizes;
- endpoint-free interface 1 alternate setting 0;
- class-driver claiming/rejection rules for the empty SCO interface;
- legacy HCI control-request compatibility;
- ACL OUT reassembly/fail-closed behavior;
- event/ACL IN validation and completion; and
- deterministic USB string/serial generation.

These tests are source-level evidence only.

## Physical acceptance still required

Source-level descriptor correctness and successful compilation do not close the driverless host gates. Physical tests must still demonstrate:

- repeated native-USB enumeration;
- descriptor/endpoint inspection on Linux;
- Linux `btusb` binding and BlueZ controller discovery;
- Windows in-box Bluetooth driver binding;
- HCI initialization through the real S3 -> WROOM path;
- BLE and Classic discovery/pairing/connection; and
- restart, suspend/resume, replug, and sustained-ACL behavior.
