#!/usr/bin/env bash
# AtaOS VirtualBox runner and verification helper (ADR-0014).
# Creates an isolated temporary VM 'ataos-verify-m1', boots the ISO, and logs serial output.
# NEVER touches or deletes any VM that does not begin with 'ataos-verify-'.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VM_NAME="ataos-verify-m1"
ISO_PATH="${1:-${ROOT_DIR}/build/debug/ataos.iso}"
LOG_FILE="${ROOT_DIR}/build/logs/vbox_serial.log"

# Locate VBoxManage on Linux/WSL or Windows host
VBOXMANAGE=""
if command -v VBoxManage >/dev/null 2>&1; then
    VBOXMANAGE="VBoxManage"
elif [ -f "/mnt/c/Program Files/Oracle/VirtualBox/VBoxManage.exe" ]; then
    VBOXMANAGE="/mnt/c/Program Files/Oracle/VirtualBox/VBoxManage.exe"
fi

if [ -z "$VBOXMANAGE" ]; then
    echo "note: VBoxManage not found. VirtualBox tests marked as NOT RUN." >&2
    echo "Install Oracle VirtualBox to run hardware virtualization checks." >&2
    exit 0
fi

if [ ! -f "$ISO_PATH" ]; then
    echo "error: ISO image not found at $ISO_PATH. Run 'make iso' first." >&2
    exit 1
fi

mkdir -p "${ROOT_DIR}/build/logs"

echo "=== AtaOS VirtualBox Verification Runner ==="
echo "VM Name:  $VM_NAME"
echo "ISO:      $ISO_PATH"
echo "Log:      $LOG_FILE"

# Check if VM already exists; if so, power off and unregister only if it matches our prefix
if "$VBOXMANAGE" list vms | grep -q "\"${VM_NAME}\""; then
    echo "Cleaning up existing test VM ${VM_NAME}..."
    "$VBOXMANAGE" controlvm "$VM_NAME" poweroff 2>/dev/null || true
    sleep 1
    "$VBOXMANAGE" unregistervm "$VM_NAME" --delete 2>/dev/null || true
fi

# Convert paths for Windows executable if running under WSL
VBOX_ISO="$ISO_PATH"
VBOX_LOG="$LOG_FILE"
if [[ "$VBOXMANAGE" == *.exe ]] && command -v wslpath >/dev/null 2>&1; then
    VBOX_ISO="$(wslpath -w "$ISO_PATH")"
    VBOX_LOG="$(wslpath -w "$LOG_FILE")"
fi

echo "Creating VM: $VM_NAME..."
"$VBOXMANAGE" createvm --name "$VM_NAME" --ostype "Other_64" --register
"$VBOXMANAGE" modifyvm "$VM_NAME" --memory 256 --cpus 1 --firmware bios
"$VBOXMANAGE" modifyvm "$VM_NAME" --uart1 0x3F8 4 --uartmode1 file "$VBOX_LOG"
"$VBOXMANAGE" storagectl "$VM_NAME" --name "IDE" --add ide
"$VBOXMANAGE" storageattach "$VM_NAME" --storagectl "IDE" --port 0 --device 0 --type dvddrive --medium "$VBOX_ISO"

echo "Starting VM (headless)..."
"$VBOXMANAGE" startvm "$VM_NAME" --type headless

echo "Running for 5 seconds..."
sleep 5

echo "Stopping VM..."
"$VBOXMANAGE" controlvm "$VM_NAME" poweroff 2>/dev/null || true
sleep 1
"$VBOXMANAGE" unregistervm "$VM_NAME" --delete 2>/dev/null || true

echo "Captured serial output in $LOG_FILE:"
if [ -f "$LOG_FILE" ]; then
    cat "$LOG_FILE"
else
    echo "(log file not found)"
fi
