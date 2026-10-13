#!/usr/bin/env bash
# AtaOS bit-for-bit reproducible build check (QR-001).
# Builds the kernel ELF in two clean directories and verifies identical SHA-256 checksums.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MODE="${1:-release}"

DIR_A="${ROOT_DIR}/build/repro_a"
DIR_B="${ROOT_DIR}/build/repro_b"

echo "=== AtaOS Reproducible Build Check ($MODE) ==="

rm -rf "$DIR_A" "$DIR_B"
mkdir -p "$DIR_A" "$DIR_B"

echo "1. Building pass A..."
make --no-print-directory -C "$ROOT_DIR" clean >/dev/null 2>&1 || true
make --no-print-directory -C "$ROOT_DIR" MODE="$MODE" kernel
cp "${ROOT_DIR}/build/${MODE}/ataos.elf" "${DIR_A}/ataos.elf"

echo "2. Building pass B..."
make --no-print-directory -C "$ROOT_DIR" clean >/dev/null 2>&1 || true
make --no-print-directory -C "$ROOT_DIR" MODE="$MODE" kernel
cp "${ROOT_DIR}/build/${MODE}/ataos.elf" "${DIR_B}/ataos.elf"

HASH_A="$(sha256sum "${DIR_A}/ataos.elf" | awk '{print $1}')"
HASH_B="$(sha256sum "${DIR_B}/ataos.elf" | awk '{print $1}')"

echo "Hash A: $HASH_A"
echo "Hash B: $HASH_B"

if [ "$HASH_A" != "$HASH_B" ]; then
    echo "FAIL: Kernel ELF builds are not bit-for-bit reproducible!" >&2
    exit 1
fi

echo "PASS: Kernel ELF is bit-for-bit reproducible."
rm -rf "$DIR_A" "$DIR_B"
exit 0
