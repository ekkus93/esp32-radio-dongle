# V1-103 / V1-104 Bring-Up Evidence Record

Use this file to record the first physical ESP32-S3 <-> ESP32-WROOM-32 UART/RTS/CTS bring-up.

Do not mark a field PASS without observed hardware evidence.

V1-102 board selection is already complete; the known board identities are prefilled below so the physical bring-up does not repeat that task.

The datasheet/source electrical preflight is recorded separately in `docs/V1_ELECTRICAL_PREFLIGHT.md` and is already PASS. This file records the physical observations still needed for V1-103/V1-104.

## Hardware identity

- Test date:
- ESP32-S3 board model: AYWHP ESP32-S3-DevKitC-1-N16R8 (ASIN `B0DG8L5NG5`)
- ESP32-S3 module marking: record from the tested unit if desired for evidence completeness
- ESP32-S3 board revision: record from the tested unit if printed
- ESP32-WROOM-32 board model: Aideepen 30-pin ESP-WROOM-32 (ASIN `B0BQJ8BTVB`)
- ESP32-WROOM module marking: record from the tested unit if desired for evidence completeness
- ESP32-WROOM board revision: record from the tested unit if printed

Board-selection evidence: `docs/V1_BOARD_VERIFICATION.md`.

## V1-103 power and ground evidence

- [x] Datasheet/source preflight confirms compatible nominal 3.3 V GPIO domains.
- [x] Static pin audit confirms the HCI GPIOs do not overlap either MCU's strapping pins.
- [x] Production/source audit confirms TX->RX and RTS->CTS directionality with no intentional output-to-output crossing.
- [ ] Each development board uses one intended power source.
- [ ] Common GND connected and verified.
- [ ] No 3.3 V rails tied together.
- [ ] No 5 V/VBUS rails tied together.
- [ ] Both MCU boards remain normally powered while the four direct HCI GPIO signals are active.
- [ ] Power-up method avoids deliberately driving a fully unpowered peer.

Power-up method used:

```text
RECORD SAFE/COMMON POWER-UP METHOD HERE
```

Common-ground continuity/observation:

```text
RECORD GROUND OBSERVATION HERE
```

## HCI wiring

- [ ] S3 GPIO4 TX -> WROOM GPIO16 RX2.
- [ ] S3 GPIO5 RX <- WROOM GPIO17 TX2.
- [ ] S3 GPIO6 RTS -> WROOM GPIO25 CTS.
- [ ] S3 GPIO7 CTS <- WROOM GPIO26 RTS.

Wiring photograph/reference:

## Smoke-test firmware

- WROOM smoke image commit:
- S3 smoke image commit:
- ESP-IDF version: v5.5.5
- UART baud: 115200

## UART configuration readback

Expected WROOM configuration line:

```text
WROOM responder: UART=2 baud=115200 TX=17 RX=16 RTS=26 CTS=25 flow=CTS_RTS threshold=96
```

Expected S3 configuration line:

```text
S3 initiator: UART=1 baud=115200 TX=4 RX=5 RTS=6 CTS=7 flow=CTS_RTS threshold=96
```

Observed WROOM line:

```text
PASTE WROOM CONFIGURATION LINE HERE
```

Observed S3 line:

```text
PASTE S3 CONFIGURATION LINE HERE
```

- [ ] WROOM reports `flow=CTS_RTS`.
- [ ] S3 reports `flow=CTS_RTS`.

## Initial complete round

### Bidirectional TX/RX evidence

Observed S3 log:

```text
PASTE S3 PING/ECHO LOG HERE
```

- [ ] 32 sequenced ping/echo frames PASS.

### WROOM RTS -> S3 CTS evidence

Measured transmit-drain time:

- `________ ms`

Observed log:

```text
PASTE PHASE-B LOG HERE
```

- [ ] Measured drain time >= 350 ms.
- [ ] 1024-byte payload integrity PASS.
- [ ] WROOM RTS -> S3 CTS asserted/throttled/released/resumed PASS.

### S3 RTS -> WROOM CTS evidence

Measured transmit-drain time:

- `________ ms`

Observed log:

```text
PASTE PHASE-C LOG HERE
```

- [ ] Measured drain time >= 350 ms.
- [ ] 1024-byte payload integrity PASS.
- [ ] S3 RTS -> WROOM CTS asserted/throttled/released/resumed PASS.

### Complete-round result

Observed S3 lines:

```text
ROUND <N> PASS: TX/RX and both RTS/CTS crossings asserted, throttled, released, and resumed
BRINGUP PASS: round=<N>
RESET TEST READY: keep both boards powered; reset either MCU and require a later round PASS
```

- [ ] Initial complete round PASS.

## Reset / boot behavior

Keep **both boards powered** for the individual reset tests below. Use the reset/EN mechanism rather than removing power from only one board.

A reset cycle counts as PASS only when the reset MCU boots normally **and a later complete S3 round passes**.

### WROOM resets while S3 stays powered

| Cycle | WROOM booted normally? | Later round number | Complete round PASS? | Notes |
| --- | --- | ---: | --- | --- |
| 1 | | | | |
| 2 | | | | |
| 3 | | | | |
| 4 | | | | |
| 5 | | | | |

- [ ] 5/5 WROOM reset cycles produce a later complete round PASS.

### S3 resets while WROOM stays powered

| Cycle | S3 booted normally? | Post-reset round number | Complete round PASS? | Notes |
| --- | --- | ---: | --- | --- |
| 1 | | | | |
| 2 | | | | |
| 3 | | | | |
| 4 | | | | |
| 5 | | | | |

- [ ] 5/5 S3 reset cycles produce a complete post-reset round PASS.

### Common cold-power cycles

Power both boards down together and bring both boards back to normal power together using the same safe/common power method used for initial bring-up.

| Cycle | Both booted normally? | Complete round PASS? | Notes |
| --- | --- | --- | --- |
| 1 | | | |
| 2 | | | |
| 3 | | | |
| 4 | | | |
| 5 | | | |

- [ ] 5/5 common cold-power cycles produce a complete round PASS.
- [ ] No boot-strap/pin-state conflict observed.
- [ ] No test deliberately drove an unpowered peer through an HCI GPIO.

## Optional logic-analyzer evidence

- Capture/reference:
- WROOM RTS -> S3 CTS transition observed: yes / no / not captured
- S3 RTS -> WROOM CTS transition observed: yes / no / not captured

## Task disposition

### V1-103 — Verify electrical assumptions

Static/datasheet preflight: **PASS** — see `docs/V1_ELECTRICAL_PREFLIGHT.md`.

Physical disposition:

- [ ] PASS

Reason/evidence summary:

### V1-104 — Verify basic UART electrical communication

- [ ] S3 -> WROOM traffic PASS.
- [ ] WROOM -> S3 traffic PASS.
- [ ] WROOM RTS -> S3 CTS assertion/release PASS.
- [ ] S3 RTS -> WROOM CTS assertion/release PASS.
- [ ] Reset/re-synchronization PASS.
- [ ] No boot-strap/pin-state conflict observed.
- [ ] **V1-104 PASS.**

Reason/evidence summary:

## Gate V1-G10

- [ ] PASS — both boards use the approved power/ground arrangement, boot reliably with the reference wiring, and pass bidirectional UART plus flow-control smoke tests.
