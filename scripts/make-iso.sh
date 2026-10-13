#!/usr/bin/env bash
# make-iso.sh - assemble the hybrid BIOS/UEFI Limine ISO.
#
# Usage: scripts/make-iso.sh KERNEL_ELF OUTPUT_ISO LIMINE_DIR STAGING_DIR CONF
# The same ISO layout serves QEMU and VirtualBox, BIOS and UEFI (docs/BOOT.md).
set -euo pipefail

if [ $# -ne 5 ]; then
    echo "usage: $0 KERNEL_ELF OUTPUT_ISO LIMINE_DIR STAGING_DIR CONF" >&2
    exit 2
fi
KERNEL="$1"; ISO="$2"; LIMINE="$3"; STAGE="$4"; CONF="$5"

for f in "$KERNEL" "$CONF" "$LIMINE/limine" "$LIMINE/limine-bios.sys" \
         "$LIMINE/limine-bios-cd.bin" "$LIMINE/limine-uefi-cd.bin" "$LIMINE/BOOTX64.EFI"; do
    [ -f "$f" ] || { echo "error: missing required file: $f" >&2; exit 1; }
done

rm -rf "$STAGE"
mkdir -p "$STAGE/boot/limine" "$STAGE/EFI/BOOT"
cp "$KERNEL" "$STAGE/boot/ataos.elf"
cp "$CONF" "$STAGE/boot/limine/limine.conf"
cp "$LIMINE/limine-bios.sys" "$LIMINE/limine-bios-cd.bin" "$LIMINE/limine-uefi-cd.bin" \
   "$STAGE/boot/limine/"
cp "$LIMINE/BOOTX64.EFI" "$STAGE/EFI/BOOT/"

# Normalise timestamps so the staging tree does not leak build time.
EPOCH="${SOURCE_DATE_EPOCH:-0}"
find "$STAGE" -exec touch -h -d "@$EPOCH" {} +

rm -f "$ISO"
SOURCE_DATE_EPOCH="$EPOCH" xorriso -as mkisofs -R -r -J \
    -b boot/limine/limine-bios-cd.bin -no-emul-boot -boot-load-size 4 -boot-info-table \
    -hfsplus -apm-block-size 2048 \
    --efi-boot boot/limine/limine-uefi-cd.bin -efi-boot-part --efi-boot-image \
    --protective-msdos-label \
    "$STAGE" -o "$ISO" >/dev/null 2>"$ISO.log" || {
        echo "error: xorriso failed; log:" >&2; cat "$ISO.log" >&2; exit 1; }

"$LIMINE/limine" bios-install "$ISO" >/dev/null
echo "ISO ready: $ISO"
