# Building ESP32 Radio Dongle V1

## Supported ESP-IDF release

V1 is pinned to **ESP-IDF v5.5.5 exactly** for both firmware targets.

The original ESP32 and ESP32-S3 are both supported by this ESP-IDF line. ESP32-S3 native USB device support is provided by the ESP-IDF TinyUSB-based USB Device Stack. The project intentionally uses one SDK version for both MCUs so the inter-MCU protocol is developed and reproduced against one known toolchain baseline.

Official references:

- <https://docs.espressif.com/projects/esp-idf/en/v5.5.5/>
- <https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32s3/api-reference/peripherals/usb_device.html>
- <https://github.com/hathach/tinyusb>

Both firmware projects contain build-time guards. Building with a different ESP-IDF release must fail visibly rather than silently producing an unqualified image.

## Install ESP-IDF v5.5.5

A typical Linux/macOS source installation is:

```sh
git clone --branch v5.5.5 --recursive https://github.com/espressif/esp-idf.git ~/esp/esp-idf-v5.5.5
cd ~/esp/esp-idf-v5.5.5
./install.sh esp32,esp32s3
. ./export.sh
idf.py --version
```

The final command must report ESP-IDF v5.5.5.

Use Espressif's documented Windows installation flow when building on Windows, but select the same v5.5.5 release.

## Firmware targets

V1 contains two independent ESP-IDF projects:

```text
firmware/
├── esp32_wroom_bt_controller/   # original ESP32 / ESP32-WROOM-32
└── esp32s3_usb_bridge/          # ESP32-S3 native USB bridge
```

Each directory is independently configurable, buildable, flashable, and cleanable with `idf.py`.

## Build the ESP32-WROOM-32 controller firmware

From the repository root:

```sh
cd firmware/esp32_wroom_bt_controller
idf.py fullclean
idf.py -DIDF_TARGET=esp32 build
```

The project also defaults `CONFIG_IDF_TARGET` to `esp32` in `sdkconfig.defaults`; the explicit command-line target above is retained in documentation and CI to make the intended target obvious.

## Build the ESP32-S3 USB bridge firmware

From the repository root:

```sh
cd firmware/esp32s3_usb_bridge
idf.py fullclean
idf.py -DIDF_TARGET=esp32s3 build
```

The project also defaults `CONFIG_IDF_TARGET` to `esp32s3` in `sdkconfig.defaults`.

## Clean-build verification

For release and gate evidence, use a clean checkout and build each target after removing all generated configuration/build state:

```sh
rm -f firmware/esp32_wroom_bt_controller/sdkconfig
rm -rf firmware/esp32_wroom_bt_controller/build
rm -f firmware/esp32s3_usb_bridge/sdkconfig
rm -rf firmware/esp32s3_usb_bridge/build

idf.py -C firmware/esp32_wroom_bt_controller -DIDF_TARGET=esp32 build
idf.py -C firmware/esp32s3_usb_bridge -DIDF_TARGET=esp32s3 build
```

GitHub Actions performs equivalent fresh-checkout builds for both targets.

## Warning policy

Project-owned C/C++ translation units are compiled with:

```text
-Wall -Wextra -Werror
```

Warnings in project-owned firmware code therefore fail the build. Third-party ESP-IDF/TinyUSB code is not globally forced through additional project warning flags because the project must not turn upstream implementation warnings into unrelated local failures.

Do not suppress a project warning with `-Wno-*` or a blanket warning pragma merely to make CI pass. Fix the warning or document a narrow, justified exception.

## Formatting policy

Project-owned C/C++ code uses the repository `.clang-format` file. Before committing C/C++ changes, run:

```sh
find firmware -type f \( -name '*.c' -o -name '*.h' -o -name '*.cpp' -o -name '*.hpp' \) \
  -print0 | xargs -0 -r clang-format --dry-run --Werror
```

To apply formatting:

```sh
find firmware -type f \( -name '*.c' -o -name '*.h' -o -name '*.cpp' -o -name '*.hpp' \) \
  -print0 | xargs -0 -r clang-format -i
```

CMake, YAML, shell, and Markdown should remain conventionally formatted and reviewable. Additional repository linters may be added as those file types become non-trivial.

## Host-only versus hardware validation

CI proves compilation, target/version guards, and host-runnable tests. It does **not** claim hardware acceptance.

The following remain hardware evidence and must not be checked off from CI alone:

- electrical UART/RTS/CTS behavior;
- ESP32 Bluetooth controller operation;
- ESP32-S3 USB enumeration on a physical host;
- Linux `btusb`/BlueZ binding;
- Windows in-box Bluetooth binding;
- real BLE/Classic pairing, traffic, recovery, and stability tests.
