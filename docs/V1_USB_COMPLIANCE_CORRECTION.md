# V1 USB Bluetooth Compliance Correction

This document records a normative correction to the original V1 specification and TODO wording.

## Reason for the correction

The original planning documents described the S3 USB Bluetooth function as a single primary-controller interface. Review against the Bluetooth Core USB Transport Layer showed that the legacy Bluetooth Controller configuration uses two interfaces:

1. the primary HCI event/ACL interface; and
2. the SCO bandwidth interface, whose default alternate setting represents zero active voice bandwidth.

V1 still does **not** implement SCO/eSCO data transport. The correction is to expose and claim the second interface in its endpoint-free alternate setting 0, not to add fake or unusable voice endpoints.

## Normative V1 USB layout

The following supersedes any earlier V1 SPEC/TODO wording that says the configuration contains only one interface or a "single primary-controller interface descriptor."

### Device descriptor

- Device class: `0xE0` Wireless Controller
- Device subclass: `0x01` Bluetooth
- Device protocol: `0x01` Primary Controller
- One USB configuration

### Configuration descriptor

- Total length: 48 bytes
- `bNumInterfaces = 2`

### Interface 0, alternate setting 0 — active HCI transport

- Class/subclass/protocol: `E0/01/01`
- 3 endpoints:
  - Event IN `0x81`, interrupt, 16-byte max packet
  - ACL OUT `0x02`, bulk, 64-byte max packet
  - ACL IN `0x82`, bulk, 64-byte max packet
- HCI commands remain host-to-device class control transfers on endpoint 0.

### Interface 1, alternate setting 0 — zero active voice channels

- Class/subclass/protocol: `E0/01/01`
- 0 endpoints
- No usable isochronous SCO transport
- No nonzero-bandwidth alternate settings in V1

The project-owned TinyUSB class shim must claim this empty second interface after claiming the primary interface. It must reject nonzero alternate settings or endpoint-bearing SCO interface forms because V1 does not implement those transports.

## HCI control-request compatibility

For the V1 single-function Bluetooth Controller, device-targeted host-to-device class requests are treated as HCI commands without depending on historical `bRequest`, `wValue`, or `wIndex` values. This covers legacy host behavior such as `bRequest=0xE0`.

Interface-targeted HCI command requests remain deliberately strict for future composite-device safety: `bRequest=0`, `wValue=0`, and `wIndex` equal to the primary interface number.

Device-to-host or non-class requests are rejected by the HCI command path.

## Implementation evidence

Production implementation:

- `firmware/esp32s3_usb_bridge/main/usb_descriptors.c`
- `firmware/esp32s3_usb_bridge/components/radio_usb_bth/radio_usb_bth.c`

Host regression coverage:

- `tests/host/test_usb_descriptors.c`
- `tests/host/test_radio_usb_bth.c`
- `tests/host/test_radio_usb_bth_control_compat.c`

The host tests validate the two-interface descriptor bytes, empty interface claim/rejection behavior, legacy HCI control-request compatibility, and the command/event/ACL transport logic.

## TODO interpretation

Until `ESP32_RADIO_DONGLE_V1_SPEC.md` and `ESP32_RADIO_DONGLE_V1_TODO.md` are mechanically rewritten, interpret their USB-interface requirements using this correction:

- any requirement for a "single primary-controller interface" is **superseded**;
- V1-402 requires the two-interface layout above;
- V1-404 must inspect both interfaces on a physical host when device tests resume; and
- V1-505 remains unchanged: SCO/eSCO data transport is intentionally unsupported in V1.

This correction does not close V1-404 or any Windows/Linux device acceptance gate. Physical USB enumeration and driver binding remain deferred.
