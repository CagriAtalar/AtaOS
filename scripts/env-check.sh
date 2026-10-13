#!/usr/bin/env bash
# env-check.sh - report the build/test environment and fail clearly when a
# required tool is missing.
#
# Usage:
#   scripts/env-check.sh                 print the full table (exit 0 unless a
#                                        tool required for building is missing)
#   scripts/env-check.sh require GROUP.. verify groups, print install hints,
#                                        exit 1 if any required tool is missing
# Groups: build  iso  hosttest  qemu
#
# Install hints are for Debian/Ubuntu (incl. WSL). Other hosts: install the
# equivalent packages with your package manager (see docs/TESTING.md).
set -u

CC_BIN="${CC_BIN:-clang}"
LD_BIN="${LD_BIN:-ld.lld}"

ver() {
    # Print the first line of "<tool> --version", or "-" if it fails.
    local out
    out="$("$@" 2>&1 | head -n 1)" || true
    printf '%s' "${out:--}"
}

have() { command -v "$1" >/dev/null 2>&1; }

# name | group | required(yes/no) | apt package | purpose
TOOLS=(
    "$CC_BIN|build|yes|clang|freestanding C compiler/assembler (ADR-0003)"
    "$LD_BIN|build|yes|lld|kernel linker (ADR-0003)"
    "make|build|yes|make|build driver"
    "bash|build|yes|bash|scripts"
    "readelf|build|no|binutils|ELF inspection in tests"
    "git|build|no|git|build identity (optional)"
    "xorriso|iso|yes|xorriso|ISO creation"
    "curl|iso|no|curl|Limine download (curl or wget)"
    "wget|iso|no|wget|Limine download (curl or wget)"
    "sha256sum|iso|yes|coreutils|Limine checksum verification"
    "tar|iso|yes|tar|Limine extraction"
    "cc|iso|yes|build-essential|builds the Limine host tool"
    "cc|hosttest|yes|build-essential|host unit tests"
    "python3|qemu|yes|python3|QEMU test runner"
    "qemu-system-x86_64|qemu|yes|qemu-system-x86|boot tests"
    "gdb|qemu|no|gdb|debugging"
    "clang-format|lint|no|clang-format|make format / format-check"
    "clang-tidy|lint|no|clang-tidy|extra lint"
    "cppcheck|lint|no|cppcheck|extra lint"
    "shellcheck|lint|no|shellcheck|script lint"
    "VBoxManage|vbox|no|virtualbox|VirtualBox helper"
)

find_ovmf() {
    for f in /usr/share/ovmf/OVMF.fd /usr/share/OVMF/OVMF.fd \
             /usr/share/OVMF/OVMF_CODE_4M.fd /usr/share/OVMF/OVMF_CODE.fd \
             /usr/share/edk2/ovmf/OVMF_CODE.fd /usr/share/qemu/OVMF.fd; do
        [ -f "$f" ] && { echo "$f"; return 0; }
    done
    return 1
}

print_table() {
    printf '%-20s | %-9s | %-8s | %-9s | %s\n' "Tool" "Available" "Required" "Group" "Version / notes"
    printf -- '---------------------+-----------+----------+-----------+----------------------------\n'
    local entry name group req pkg purpose avail v
    for entry in "${TOOLS[@]}"; do
        IFS='|' read -r name group req pkg purpose <<<"$entry"
        if have "$name"; then avail=yes; v="$(ver "$name" --version)"; else avail=NO; v="$purpose"; fi
        printf '%-20s | %-9s | %-8s | %-9s | %s\n' "$name" "$avail" "$req" "$group" "$v"
    done
    local ovmf
    if ovmf="$(find_ovmf)"; then
        printf '%-20s | %-9s | %-8s | %-9s | %s\n' "OVMF (UEFI fw)" yes "uefi-test" qemu "$ovmf"
    else
        printf '%-20s | %-9s | %-8s | %-9s | %s\n' "OVMF (UEFI fw)" NO "uefi-test" qemu "UEFI tests will be NOT RUN (apt: ovmf)"
    fi
}

# require GROUP... : exit 1 and explain if any required tool of the group is missing.
require() {
    local missing=0 entry name group req pkg purpose g
    for g in "$@"; do
        for entry in "${TOOLS[@]}"; do
            IFS='|' read -r name group req pkg purpose <<<"$entry"
            [ "$group" = "$g" ] || continue
            [ "$req" = yes ] || continue
            if ! have "$name"; then
                echo "error: required tool '$name' not found (needed for: $purpose)." >&2
                echo "       Debian/Ubuntu/WSL: sudo apt install $pkg" >&2
                missing=1
            fi
        done
        if [ "$g" = iso ] && ! have curl && ! have wget; then
            echo "error: neither 'curl' nor 'wget' found (needed to download Limine)." >&2
            echo "       Debian/Ubuntu/WSL: sudo apt install curl" >&2
            missing=1
        fi
    done
    return $missing
}

case "${1:-}" in
    require)
        shift
        require "$@" || exit 1
        ;;
    "")
        print_table
        echo
        if have "$CC_BIN" && have "$LD_BIN"; then
            tmp="$(mktemp -d)"
            echo 'void f(void){}' >"$tmp/t.c"
            if "$CC_BIN" --target=x86_64-unknown-none-elf -ffreestanding -c "$tmp/t.c" -o "$tmp/t.o" 2>"$tmp/err" &&
               "$LD_BIN" -r "$tmp/t.o" -o "$tmp/t2.o" 2>>"$tmp/err"; then
                echo "freestanding x86_64 compile+link probe: OK"
            else
                echo "freestanding x86_64 compile+link probe: FAILED"
                cat "$tmp/err"
            fi
            rm -rf "$tmp"
        fi
        require build || exit 1
        ;;
    *)
        echo "usage: $0 [require GROUP...]" >&2
        exit 2
        ;;
esac
