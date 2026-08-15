#!/usr/bin/env bash
set -euo pipefail

s3=firmware/esp32s3_usb_bridge/main/s3_bridge.c
wroom=firmware/esp32_wroom_bt_controller/main/wroom_bridge.c
smoke=firmware/components/radio_uart_smoke/radio_uart_smoke.c

require_line() {
  local file=$1
  local text=$2
  if ! grep -Fqx "$text" "$file"; then
    echo "error: electrical contract mismatch in $file: expected '$text'" >&2
    exit 1
  fi
}

# Production S3 role: TX4, RX5, RTS6, CTS7.
require_line "$s3" '#define S3_HCI_TX_GPIO 4'
require_line "$s3" '#define S3_HCI_RX_GPIO 5'
require_line "$s3" '#define S3_HCI_RTS_GPIO 6'
require_line "$s3" '#define S3_HCI_CTS_GPIO 7'
grep -Fq '.flow_ctrl = UART_HW_FLOWCTRL_CTS_RTS' "$s3"

# Production WROOM role: TX17, RX16, RTS26, CTS25.
require_line "$wroom" '#define WROOM_HCI_TX_GPIO 17'
require_line "$wroom" '#define WROOM_HCI_RX_GPIO 16'
require_line "$wroom" '#define WROOM_HCI_RTS_GPIO 26'
require_line "$wroom" '#define WROOM_HCI_CTS_GPIO 25'
grep -Fq '.flow_ctrl = UART_HW_FLOWCTRL_CTS_RTS' "$wroom"

# Dedicated bring-up images must exercise the same pin contract.
for line in \
  '#define SMOKE_TX_GPIO 4' \
  '#define SMOKE_RX_GPIO 5' \
  '#define SMOKE_RTS_GPIO 6' \
  '#define SMOKE_CTS_GPIO 7' \
  '#define SMOKE_TX_GPIO 17' \
  '#define SMOKE_RX_GPIO 16' \
  '#define SMOKE_RTS_GPIO 26' \
  '#define SMOKE_CTS_GPIO 25'; do
  require_line "$smoke" "$line"
done
grep -Fq '.flow_ctrl = UART_HW_FLOWCTRL_CTS_RTS' "$smoke"

# Pin choices intentionally avoid documented strapping pins.
s3_link=(4 5 6 7)
s3_straps=(0 3 45 46)
wroom_link=(16 17 25 26)
wroom_straps=(0 2 5 12 15)

for link_pin in "${s3_link[@]}"; do
  for strap_pin in "${s3_straps[@]}"; do
    if [[ "$link_pin" == "$strap_pin" ]]; then
      echo "error: S3 HCI pin GPIO$link_pin overlaps an ESP32-S3 strapping pin" >&2
      exit 1
    fi
  done
done

for link_pin in "${wroom_link[@]}"; do
  for strap_pin in "${wroom_straps[@]}"; do
    if [[ "$link_pin" == "$strap_pin" ]]; then
      echo "error: WROOM HCI pin GPIO$link_pin overlaps an original-ESP32 strapping pin" >&2
      exit 1
    fi
  done
done

echo "electrical pin/direction contract passed"
