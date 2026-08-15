#!/usr/bin/env bash
set -euo pipefail

production_paths=(
  firmware/esp32_wroom_bt_controller
  firmware/esp32s3_usb_bridge
)

# Production firmware must never dump arbitrary HCI/ACL/event buffers to logs.
# These APIs are useful during ad-hoc debugging but can expose Bluetooth payloads,
# pairing traffic, or other security-sensitive material if they reach a release.
forbidden_regex='ESP_LOG_BUFFER_(HEX|HEXDUMP|CHAR)|esp_log_buffer_(hex|hexdump|char)'

if grep -RInE \
  --include='*.c' \
  --include='*.h' \
  --include='*.cpp' \
  --include='*.hpp' \
  "${forbidden_regex}" \
  "${production_paths[@]}"; then
  echo "error: production firmware contains a forbidden buffer-dump logging API" >&2
  exit 1
fi

# Release overlays are part of the security/timing contract. Verify the source
# files retain the authoritative Kconfig choice symbols. ESP-IDF derives the
# numeric WARN level from LOG_DEFAULT_LEVEL_WARN and makes the maximum equal to
# that level when LOG_MAXIMUM_EQUALS_DEFAULT is selected.
for release_config in \
  firmware/esp32_wroom_bt_controller/sdkconfig.release \
  firmware/esp32s3_usb_bridge/sdkconfig.release; do
  grep -qx 'CONFIG_LOG_DEFAULT_LEVEL_WARN=y' "${release_config}"
  grep -qx 'CONFIG_LOG_MAXIMUM_EQUALS_DEFAULT=y' "${release_config}"
  grep -qx '# CONFIG_LOG_COLORS is not set' "${release_config}"
  grep -qx 'CONFIG_BOOTLOADER_LOG_LEVEL_WARN=y' "${release_config}"
  grep -qx 'CONFIG_APP_REPRODUCIBLE_BUILD=y' "${release_config}"
done

echo "release logging policy passed"
