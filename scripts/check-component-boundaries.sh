#!/usr/bin/env bash
set -euo pipefail

production_projects=(
  firmware/esp32_wroom_bt_controller/CMakeLists.txt
  firmware/esp32s3_usb_bridge/CMakeLists.txt
)

for cmake_file in "${production_projects[@]}"; do
  grep -q 'components/radio_h4' "${cmake_file}"
  if grep -q 'components/radio_uart_smoke' "${cmake_file}"; then
    echo "error: production project imports development-only radio_uart_smoke: ${cmake_file}" >&2
    exit 1
  fi
  if grep -Eq 'EXTRA_COMPONENT_DIRS[^
]*["'"']\.\./components["'"']' "${cmake_file}"; then
    echo "error: production project broadly imports all shared components: ${cmake_file}" >&2
    exit 1
  fi
done

for cmake_file in \
  firmware/bringup/esp32_wroom_uart_smoke/CMakeLists.txt \
  firmware/bringup/esp32s3_uart_smoke/CMakeLists.txt; do
  grep -q 'components/radio_h4' "${cmake_file}"
  grep -q 'components/radio_uart_smoke' "${cmake_file}"
done

echo "component boundary policy passed"
