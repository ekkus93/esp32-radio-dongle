# V1 UART / RTS-CTS Physical Bring-Up

This procedure supplies the physical evidence for V1-103 and V1-104 in `ESP32_RADIO_DONGLE_V1_TODO.md`.

Do not use the production Bluetooth firmware for this first electrical test. The repository contains two dedicated bring-up images:

- `firmware/bringup/esp32_wroom_uart_smoke/` — WROOM responder
- `firmware/bringup/esp32s3_uart_smoke/` — S3 initiator

Both use the same 115200-baud inter-MCU configuration as the V1 production firmware and configure UART hardware RTS/CTS.

Read `docs/V1_ELECTRICAL_PREFLIGHT.md` before performing this procedure. The direct HCI GPIO link is qualified only with both MCU I/O domains powered normally; do not deliberately drive an unpowered peer.

## 1. Required physical wiring

The V1 reference interconnect is:

| ESP32-S3 | Direction | ESP32-WROOM-32 |
| --- | --- | --- |
| GPIO4 TX | -> | GPIO16 RX2 |
| GPIO5 RX | <- | GPIO17 TX2 |
| GPIO6 RTS | -> | GPIO25 CTS |
| GPIO7 CTS | <- | GPIO26 RTS |
| GND | <-> | GND |

ASCII view:

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

- Connect GND between the boards.
- Do **not** connect the two development boards' 3.3 V pins together.
- Do **not** connect their 5 V/VBUS pins together for this bring-up.
- Give each board one normal power source; do not also feed a board from a header supply while its USB power is active unless that exact board explicitly supports it.
- The HCI signals are 3.3 V logic. Do not insert a 5 V UART adapter in this link.
- Do not deliberately leave one MCU board completely unpowered while the other actively drives the four HCI signal wires.

### Recommended initial power-up method

Preferred bench arrangement:

1. With the USB hub/power source switched **off**, connect both boards' USB cables.
2. With both boards unpowered, connect and re-check GND plus the four HCI signal wires.
3. Use one switched hub/power control to energize both development-board USB feeds together.
4. Confirm both boards reach their normal smoke-firmware startup state before beginning the test.

If a common switched USB source is not available, do not intentionally leave a fully booted peer driving an unpowered board. Use a bench arrangement that keeps both boards normally powered before the direct HCI GPIO link is exercised.

The previous procedure's deliberate WROOM-first/S3-second cold-power test is no longer part of V1 acceptance because the GPIO interface is not being claimed as power-off tolerant.

## 2. Flash the dedicated WROOM smoke image

With ESP-IDF v5.5.5 active:

```bash
cd firmware/bringup/esp32_wroom_uart_smoke
idf.py fullclean
idf.py set-target esp32
idf.py build
idf.py -p <WROOM_SERIAL_PORT> flash monitor
```

Examples of `<WROOM_SERIAL_PORT>` on Linux commonly look like `/dev/ttyUSB0` or `/dev/ttyACM0`; identify the actual port on the test host rather than assuming a fixed name.

Expected WROOM log after boot:

```text
V1-103/V1-104 ESP32-WROOM UART/RTS/CTS bring-up image
WROOM responder: UART=2 baud=115200 TX=17 RX=16 RTS=26 CTS=25
READY: waiting for ESP32-S3 smoke-test initiator
```

Leave the WROOM powered and its monitor running.

## 3. Flash the dedicated S3 smoke image

In a second terminal with ESP-IDF v5.5.5 active:

```bash
cd firmware/bringup/esp32s3_uart_smoke
idf.py fullclean
idf.py set-target esp32s3
idf.py build
idf.py -p <S3_DEBUG_SERIAL_PORT> flash monitor
```

For an S3 development board with separate USB-to-UART and native USB connectors, use the normal flashing/debug connector for this smoke-test log. The native USB device connector is not needed until V1-404/V1-601.

## 4. What the automated smoke test proves

The S3 initiator performs three phases.

### Phase A — bidirectional TX/RX

The S3 sends 32 sequenced ping frames and the WROOM echoes each frame byte-for-byte.

A pass proves the basic signal path is correct in both directions:

- S3 GPIO4 TX -> WROOM GPIO16 RX
- WROOM GPIO17 TX -> S3 GPIO5 RX

Expected S3 log:

```text
PASS: 32 bidirectional ping/echo frames
```

### Phase B — WROOM RTS -> S3 CTS

The WROOM acknowledges the phase, deliberately pauses its UART RX interrupt for 600 ms, and then receives a 1024-byte deterministic payload from the S3.

With hardware flow control working, the WROOM RX FIFO reaches its configured RTS threshold, WROOM RTS throttles the S3 CTS-controlled transmitter, and the S3's transmit-drain wait stretches for hundreds of milliseconds. After RX resumes, all bytes must arrive intact.

Expected S3 log:

```text
PASS: WROOM RTS -> S3 CTS backpressure, 1024-byte payload drained in <N> ms
```

The automated threshold requires the measured drain time to be at least 350 ms.

### Phase C — S3 RTS -> WROOM CTS

The reverse test deliberately pauses S3 UART RX for 600 ms while the WROOM sends the same 1024-byte pattern. The WROOM measures how long its CTS-controlled transmit path takes to drain and reports that timing back to the S3.

Expected S3 log:

```text
PASS: S3 RTS -> WROOM CTS backpressure, 1024-byte payload drained in <N> ms
BRINGUP PASS: TX/RX and both RTS/CTS crossings are functional
```

The automated threshold again requires at least 350 ms of measured drain time.

## 5. Required V1-103/V1-104 evidence

Copy or photograph the following into the bring-up record before marking the tasks complete:

1. Exact ESP32-S3 board/module identification.
2. Exact ESP32-WROOM-32 board/module identification.
3. Photograph or wiring record showing all five interconnects.
4. Confirmation that each board used one intended power source and no 3.3 V or 5 V rails were tied board-to-board.
5. Confirmation that both boards were normally powered while the direct HCI GPIO link was active.
6. Description of the common/safe power-up method used.
7. WROOM boot log showing its UART pin configuration and `READY` state.
8. S3 log showing all three `PASS` phases and final `BRINGUP PASS`.
9. The two measured RTS/CTS drain times.
10. Reset/boot test results described below.

## 6. Reset / boot-strap conflict check

After the first full pass, leave both boards powered and leave the five interconnect wires installed.

Run these checks:

1. Press/reset the WROOM five times while the S3 remains powered.
2. Press/reset the S3 five times while the WROOM remains powered.
3. Verify each reset uses the board reset/EN mechanism; do not remove power from one MCU while the peer remains powered and driving the HCI wires.
4. Power **both boards down together**, then power **both boards up together** five times using the same safe/common power method used for initial bring-up.
5. Verify each board reaches its normal smoke-test startup log on every cycle.
6. Re-run at least one complete smoke-test pass after the reset sequence.

Record any boot-loop, ROM-download-mode entry, UART garbage, or failure to reach the smoke-test application. Any such event keeps V1-104 open until understood.

### Independent full power removal is a separate requirement

V1-G10 does not claim that one raw ESP32 GPIO endpoint may remain driven while the other MCU's I/O domain is unpowered. If later acceptance requires physically removing power from one board while the other remains active and wired, first define/qualify isolation or sequencing hardware for that condition.

A reset with VDD still present is not the same electrical condition as removing board power.

## 7. Optional logic-analyzer confirmation

The automated test provides functional RTS/CTS evidence. If a logic analyzer or oscilloscope is available, a stronger trace can be captured on:

- S3 GPIO6 / WROOM GPIO25 during Phase C
- WROOM GPIO26 / S3 GPIO7 during Phase B

The trace should show the receiver's RTS changing state during the deliberate RX pause and the peer TX stream stopping until the receiver releases backpressure.

This trace is useful evidence but is not required if the automated timing and payload-integrity checks pass consistently.

## 8. Failure interpretation

### S3 never synchronizes with WROOM

Check, in order:

1. common GND;
2. both boards are normally powered;
3. TX/RX crossing (GPIO4 -> GPIO16 and GPIO17 -> GPIO5);
4. RTS/CTS crossing (GPIO6 -> GPIO25 and GPIO26 -> GPIO7);
5. both images are the dedicated smoke-test builds;
6. both images use the shared 115200-baud configuration;
7. exact board variants and pin accessibility from `V1_BOARD_VERIFICATION.md`.

### Ping passes but Phase B fails

Basic TX/RX is correct. Focus on WROOM GPIO26 RTS -> S3 GPIO7 CTS and the board-level availability of those pins.

### Phase B passes but Phase C fails

Focus on S3 GPIO6 RTS -> WROOM GPIO25 CTS.

### Payload corrupts after deliberate RX pause

Treat this as a real flow-control failure. Do not increase baud rate and do not mark V1-104 complete. Capture both serial logs and, if available, an RTS/CTS logic trace.

### Test reports too little blocking time

The data path may be working while hardware backpressure is not actually being exercised. Verify the RTS/CTS wires and exact board pin mappings before changing the test threshold.

## 9. Restoring production firmware

The smoke-test images are bring-up tools only. After V1-103/V1-104 passes, reflash:

- `firmware/esp32_wroom_bt_controller/` on the WROOM; and
- `firmware/esp32s3_usb_bridge/` on the S3.

Then continue with V1-306 (raw controller/HCI verification) and physical USB enumeration work.
