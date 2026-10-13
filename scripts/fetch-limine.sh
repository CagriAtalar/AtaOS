#!/usr/bin/env bash
# fetch-limine.sh - download, verify and unpack the pinned Limine binary release.
#
# Pin (ADR-0001): Limine v12.9.3, asset limine-binary.tar.gz.
# The archive is verified against SHA-256 BEFORE it is extracted; a mismatch
# deletes nothing but aborts with an error. Output (all under build/):
#   build/downloads/limine-binary.tar.gz
#   build/limine/...   (extracted release + host 'limine' tool built from limine.c)
#
# Usage: scripts/fetch-limine.sh [build-dir]      (default: build)
# Environment: LIMINE_OFFLINE=1 refuses to download (uses cached archive only).
set -euo pipefail

LIMINE_VERSION="v12.9.3"
LIMINE_SHA256="9f42fe9ea2e84d71056529969b1d5c24ba9a2287ffcc01bcce58b0ef8aa98b1d"
LIMINE_URL="https://github.com/Limine-Bootloader/Limine/releases/download/${LIMINE_VERSION}/limine-binary.tar.gz"

BUILD_DIR="${1:-build}"
DL_DIR="$BUILD_DIR/downloads"
OUT_DIR="$BUILD_DIR/limine"
ARCHIVE="$DL_DIR/limine-binary.tar.gz"
STAMP="$OUT_DIR/.stamp-$LIMINE_SHA256"

if [ -f "$STAMP" ] && [ -x "$OUT_DIR/limine" ]; then
    exit 0
fi

mkdir -p "$DL_DIR"

verify() {
    local got
    got="$(sha256sum "$1" | cut -d' ' -f1)"
    [ "$got" = "$LIMINE_SHA256" ]
}

if [ ! -f "$ARCHIVE" ] || ! verify "$ARCHIVE"; then
    if [ "${LIMINE_OFFLINE:-0}" = 1 ]; then
        echo "error: no valid cached Limine archive at $ARCHIVE and LIMINE_OFFLINE=1" >&2
        exit 1
    fi
    echo "Downloading Limine $LIMINE_VERSION ..."
    tmp="$ARCHIVE.part"
    if command -v curl >/dev/null 2>&1; then
        curl -fsSL --retry 3 -o "$tmp" "$LIMINE_URL"
    elif command -v wget >/dev/null 2>&1; then
        wget -q -O "$tmp" "$LIMINE_URL"
    else
        echo "error: need curl or wget to download $LIMINE_URL" >&2
        exit 1
    fi
    if ! verify "$tmp"; then
        got="$(sha256sum "$tmp" | cut -d' ' -f1)"
        echo "error: SHA-256 mismatch for Limine download" >&2
        echo "  expected: $LIMINE_SHA256" >&2
        echo "  actual:   $got" >&2
        echo "  (file left at $tmp for inspection; it will NOT be used)" >&2
        exit 1
    fi
    mv "$tmp" "$ARCHIVE"
fi

rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR"
tar -xzf "$ARCHIVE" -C "$OUT_DIR" --strip-components=1

# Build the host-side installer tool from the release's single-file source.
cc -O2 -std=c99 -o "$OUT_DIR/limine" "$OUT_DIR/limine.c"

touch "$STAMP"
echo "Limine $LIMINE_VERSION ready in $OUT_DIR"
