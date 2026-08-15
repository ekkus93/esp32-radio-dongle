#!/usr/bin/env bash
set -euo pipefail

required_docs=(
  README.md
  docs/ESP32_RADIO_DONGLE_V1_SPEC.md
  docs/ESP32_RADIO_DONGLE_V1_TODO.md
  docs/BUILDING.md
  docs/FLASHING.md
  docs/HARDWARE.md
  docs/USB_BLUETOOTH_V1.md
  docs/USAGE.md
  docs/TROUBLESHOOTING.md
  docs/LIMITATIONS.md
  docs/V1_BOARD_VERIFICATION.md
  docs/V1_EVIDENCE_INDEX.md
  docs/V1_RELEASE_CONFIGURATION.md
  docs/V1_SECURITY_REVIEW.md
  docs/V1_UART_BRINGUP.md
  docs/V1_UART_BRINGUP_EVIDENCE.md
  docs/V1_USB_COMPLIANCE_CORRECTION.md
  docs/V1_DOCUMENTATION_EVIDENCE_AUDIT.md
)

for path in "${required_docs[@]}"; do
  if [[ ! -f "${path}" ]]; then
    echo "error: required V1 documentation is missing: ${path}" >&2
    exit 1
  fi
done

spec=docs/ESP32_RADIO_DONGLE_V1_SPEC.md
todo=docs/ESP32_RADIO_DONGLE_V1_TODO.md
limits=docs/LIMITATIONS.md
board=docs/V1_BOARD_VERIFICATION.md
audit=docs/V1_DOCUMENTATION_EVIDENCE_AUDIT.md

# Current locked contract must be represented in the canonical SPEC/TODO.
grep -Fq 'ESP-IDF **v5.5.5 exactly**' "${spec}"
grep -Fq 'legacy Bluetooth Controller **two-interface** configuration' "${spec}"
grep -Fq 'SCO/eSCO synchronous voice transport is intentionally **out of V1 scope**' "${spec}"
grep -Fq 'project-owned `radio_usb_bth` TinyUSB application class shim' "${spec}"
grep -Fq 'AYWHP ESP32-S3-DevKitC-1-N16R8' "${spec}"
grep -Fq 'Aideepen 30-pin ESP-WROOM-32' "${spec}"

grep -Fq '[x] **V1-102 — Verify selected development boards**' "${todo}"
grep -Fq '[x] **V1-1104 — Release configuration review**' "${todo}"
grep -Fq '**Gate V1-G110: PASS for software configuration.**' "${todo}"
grep -Fq 'AYWHP ESP32-S3-DevKitC-1-N16R8' "${board}"
grep -Fq 'Aideepen 30-pin ESP-WROOM-32' "${board}"
grep -Fq 'Software documentation/evidence audit: **PASS**.' "${audit}"
grep -Fq 'The exact initial V1 development boards **are identified and approved for the reference pin contract**' "${limits}"

# Historical wording that is no longer a valid V1 requirement must not return.
for forbidden in \
  'SCO/eSCO voice support SHALL be treated separately' \
  'whether SCO/eSCO voice transport is a V1 release requirement or a documented post-V1 enhancement' \
  'Implement the single primary-controller interface descriptor' \
  'exact ESP32-S3 and ESP32-WROOM-32 development-board models for initial physical acceptance have not yet been recorded'; do
  if grep -RFn --exclude=V1_USB_COMPLIANCE_CORRECTION.md -- "${forbidden}" README.md docs; then
    echo "error: stale V1 documentation contract text found: ${forbidden}" >&2
    exit 1
  fi
done

# Software evidence must not claim physical gate completion while device tests are deferred.
for forbidden_gate in \
  'Gate V1-G10: PASS' \
  'Gate V1-G30: PASS' \
  'Gate V1-G40: PASS' \
  'Gate V1-G50: PASS' \
  'Gate V1-G60: PASS' \
  'Gate V1-G70: PASS' \
  'Gate V1-G80: PASS' \
  'Gate V1-G90: PASS' \
  'Gate V1-G100: PASS' \
  'Gate V1-G120: PASS' \
  'Gate V1-FINAL: PASS'; do
  if grep -Fq "${forbidden_gate}" "${todo}"; then
    echo "error: deferred physical gate is incorrectly documented as PASS: ${forbidden_gate}" >&2
    exit 1
  fi
done

echo "V1 documentation contract passed"
