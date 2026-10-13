#!/usr/bin/env bash
# Test suite for scripts/fetch-limine.sh and SHA-256 integrity (ADR-0001).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="${SCRIPT_DIR}/../.."

echo "=== Testing Limine Fetch and Checksum Verification ==="

TMP_BUILD="$(mktemp -d)"
trap 'rm -rf "$TMP_BUILD"' EXIT

bash "${ROOT_DIR}/scripts/fetch-limine.sh" "$TMP_BUILD"

if [ ! -f "${TMP_BUILD}/limine/limine" ]; then
    echo "FAIL: limine executable not created in ${TMP_BUILD}/limine" >&2
    exit 1
fi

if [ ! -f "${TMP_BUILD}/limine/limine-bios.sys" ]; then
    echo "FAIL: limine-bios.sys not found" >&2
    exit 1
fi

if [ ! -f "${TMP_BUILD}/limine/BOOTX64.EFI" ]; then
    echo "FAIL: BOOTX64.EFI not found" >&2
    exit 1
fi

echo "PASS: Limine downloaded, verified against SHA-256, and extracted properly."
exit 0
