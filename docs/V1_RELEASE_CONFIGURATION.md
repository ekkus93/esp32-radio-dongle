# ESP32 Radio Dongle V1 Release Configuration

This document defines the software release configuration for V1. Hardware and host acceptance remain separate gates in `ESP32_RADIO_DONGLE_V1_TODO.md`.

## Release firmware targets

Only these two projects are V1 production/release firmware:

- `firmware/esp32_wroom_bt_controller/`
- `firmware/esp32s3_usb_bridge/`

The projects under `firmware/bringup/` are development-only diagnostic images. They are never V1 release artifacts and must not be substituted for either production image during final acceptance.

Production CMake files explicitly import only `firmware/components/radio_h4`; they do not broadly import the bring-up-only `radio_uart_smoke` component. `scripts/check-component-boundaries.sh` enforces that separation in CI.

## ESP-IDF baseline

Release builds use ESP-IDF **v5.5.5**, the same pinned SDK used by development CI.

Each production project has two configuration layers:

1. `sdkconfig.defaults` — functional V1 defaults.
2. `sdkconfig.release` — release-only logging/reproducibility overlay.

ESP-IDF supports multiple sdkconfig-default files through `SDKCONFIG_DEFAULTS`; later files override earlier default values. The release build therefore applies:

```text
sdkconfig.defaults;sdkconfig.release
```

## Local release build commands

Start from a clean checkout with ESP-IDF v5.5.5 activated.

### ESP32-WROOM-32 controller

```bash
cd firmware/esp32_wroom_bt_controller
rm -f sdkconfig
SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.release" idf.py fullclean build
```

### ESP32-S3 USB bridge

```bash
cd firmware/esp32s3_usb_bridge
rm -f sdkconfig
SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.release" idf.py fullclean build
```

Removing `sdkconfig` before the release build is intentional: `sdkconfig.defaults` files are defaults, not overrides of an existing `sdkconfig`.

## Release logging policy

The release overlay selects the authoritative ESP-IDF Kconfig choices:

```text
CONFIG_LOG_DEFAULT_LEVEL_WARN=y
CONFIG_LOG_MAXIMUM_EQUALS_DEFAULT=y
# CONFIG_LOG_COLORS is not set
CONFIG_BOOTLOADER_LOG_LEVEL_WARN=y
```

ESP-IDF derives the numeric application default from `LOG_DEFAULT_LEVEL_WARN`. Selecting `LOG_MAXIMUM_EQUALS_DEFAULT` makes the maximum compiled/runtime-selectable level follow that default rather than maintaining a separate hand-written numeric setting. The bootloader WARN choice similarly determines its numeric verbosity internally.

Consequences:

- WARN and ERROR messages remain available for actionable fault diagnosis.
- INFO, DEBUG, and VERBOSE application log calls are excluded above the configured maximum level.
- Normal periodic bridge counter/state INFO output is therefore not emitted by a release image.
- Bootloader output is reduced to warnings/errors.
- ANSI log colors are disabled.

The release build does **not** rely only on a runtime log-level call. The maximum level is constrained by the Kconfig `LOG_MAXIMUM_EQUALS_DEFAULT` choice.

## Security/log-content audit

The production firmware logging paths were reviewed for V1-1104.

Current production logs contain only categories such as:

- state changes;
- UART pin/baud configuration;
- controller version/manufacturer metadata;
- queue/counter summaries;
- USB lifecycle counts;
- transport/recovery reasons; and
- startup/build information.

They do not intentionally log:

- HCI command payload bytes;
- HCI event payload bytes;
- ACL payload bytes;
- Bluetooth link keys;
- pairing PIN/passkey material;
- LTK/IRK/CSRK values;
- application GATT payloads; or
- arbitrary packet hex dumps.

`scripts/check-release-logging.sh` enforces a minimum regression policy by failing if ESP-IDF buffer/hex-dump logging APIs appear in either production firmware tree. This does not replace code review for newly introduced hand-written byte-by-byte logging; security-sensitive logging changes still require review.

## Development diagnostics

Development builds intentionally retain INFO-level state and counter diagnostics because they are needed during initial hardware bring-up, host enumeration debugging, queue-pressure testing, and stability characterization.

Development-only assets include:

- `firmware/bringup/esp32_wroom_uart_smoke/`
- `firmware/bringup/esp32s3_uart_smoke/`
- `firmware/components/radio_uart_smoke/`
- `docs/V1_UART_BRINGUP.md`
- `docs/V1_UART_BRINGUP_EVIDENCE.md`

None of those create a host-side dependency for normal V1 Bluetooth use. They are engineering/acceptance tools only.

## CI release artifacts

`.github/workflows/firmware-ci.yml` builds both production targets with the release overlay in dedicated release jobs. The job verifies the generated `sdkconfig` contains the required authoritative release choice symbols.

For every successful release-profile build, CI generates `build/SHA256SUMS.txt` covering all `.bin` flash images and uploads a commit-addressed artifact:

```text
v1-release-esp32-controller-<git-sha>
v1-release-esp32s3-bridge-<git-sha>
```

Each artifact includes, where produced by ESP-IDF:

- application `.bin`;
- bootloader `.bin`;
- partition-table `.bin`;
- `flash_args`;
- `flasher_args.json`; and
- `SHA256SUMS.txt`.

The final V1 release must capture the artifacts from the exact hardware-qualified release commit. CI artifacts from earlier development commits are evidence that the release pipeline works, but they are not the final release inputs.

## Reproducibility setting

The release overlay enables:

```text
CONFIG_APP_REPRODUCIBLE_BUILD=y
```

This asks ESP-IDF to avoid build metadata that unnecessarily changes otherwise equivalent application images. The final release record must still preserve the exact git commit and SHA-256 hashes rather than assuming two separately produced binaries are interchangeable.

## V1-1104 disposition

Software-side release configuration is complete when all of the following are true:

- both `sdkconfig.release` files exist;
- CI builds both release profiles;
- CI verifies WARN-only default/maximum logging through the authoritative Kconfig choices;
- the release logging policy check passes;
- production logs contain no known pairing/key/payload dumps; and
- development-only bring-up images are excluded from production component discovery and release artifacts.

Hardware timing impact of logging under sustained Bluetooth traffic remains part of V1-904 and is intentionally deferred until device testing resumes.
