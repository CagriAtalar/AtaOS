#!/usr/bin/env python3
"""
AtaOS Headless QEMU Smoke Test Runner (ADR-0014).
Runs BIOS and UEFI tests, asserts serial output markers, and validates ISA debug-exit codes.
"""

import argparse
import os
import subprocess
import sys
import time

def find_ovmf():
    candidates = [
        "/usr/share/OVMF/OVMF.fd",
        "/usr/share/ovmf/OVMF.fd",
        "/usr/share/OVMF/OVMF_CODE_4M.fd",
        "/usr/share/edk2-ovmf/x64/OVMF.fd",
    ]
    for path in candidates:
        if os.path.isfile(path):
            return path
    return None

def run_qemu_test(name, qemu_cmd, expected_markers, expected_exit, timeout_sec, log_path):
    print(f"Running test: {name} ... ", end="", flush=True)
    with open(log_path, "w") as log_file:
        try:
            start_time = time.time()
            proc = subprocess.run(
                qemu_cmd,
                stdout=log_file,
                stderr=subprocess.STDOUT,
                timeout=timeout_sec
            )
            duration = time.time() - start_time
            exit_code = proc.returncode
        except subprocess.TimeoutExpired:
            print("FAILED (Timeout)")
            print(f"  Log output captured in: {log_path}")
            return False

    with open(log_path, "r", errors="replace") as f:
        output = f.read()

    # Check exit code
    if exit_code != expected_exit:
        print(f"FAILED (Exit code {exit_code}, expected {expected_exit})")
        print(f"--- Serial Log Output ---")
        print(output.strip())
        print(f"-------------------------")
        return False

    # Check required markers
    for marker in expected_markers:
        if marker not in output:
            print(f"FAILED (Missing marker: '{marker}')")
            print(f"--- Serial Log Output ---")
            print(output.strip())
            print(f"-------------------------")
            return False

    print(f"PASS ({duration:.2f}s, exit code {exit_code})")
    return True

def main():
    parser = argparse.ArgumentParser(description="AtaOS QEMU Test Runner")
    parser.add_argument("--root", default=".", help="Project root directory")
    parser.add_argument("--mode", default="debug", help="Build mode")
    parser.add_argument("--log-dir", default="build/logs", help="Directory for test logs")
    args = parser.parse_args()

    os.makedirs(args.log_dir, exist_ok=True)

    iso_test = os.path.join(args.root, "build", f"{args.mode}-test", "ataos.iso")
    iso_panic = os.path.join(args.root, "build", f"{args.mode}-testpanic", "ataos.iso")
    iso_fault = os.path.join(args.root, "build", f"{args.mode}-testfault", "ataos.iso")

    if not os.path.isfile(iso_test):
        print(f"error: test ISO not found at {iso_test}. Run 'make TEST=1 iso' first.")
        return 1

    common_qemu = [
        "qemu-system-x86_64",
        "-machine", "q35",
        "-m", "256M",
        "-cpu", "qemu64",
        "-smp", "1",
        "-display", "none",
        "-serial", "stdio",
        "-device", "isa-debug-exit,iobase=0xf4,iosize=0x04",
        "-no-reboot",
    ]

    tests_passed = 0
    tests_failed = 0
    tests_skipped = 0

    print("=== AtaOS Headless QEMU Smoke Test Suite ===")

    # Test 1: BIOS Boot Smoke Test
    cmd_bios = common_qemu + ["-cdrom", iso_test]
    log_bios = os.path.join(args.log_dir, "qemu_bios_boot.log")
    markers_boot = [
        "[ataos] AtaOS 0.1.0",
        "[ataos] bootloader: Limine",
        "[ataos] hhdm base: 0xffff800000000000",
        "[ataos] boot complete",
    ]
    if run_qemu_test("BIOS Boot Smoke Test", cmd_bios, markers_boot, 33, 10, log_bios):
        tests_passed += 1
    else:
        tests_failed += 1

    # Test 2: UEFI Boot Smoke Test
    ovmf_path = find_ovmf()
    if ovmf_path:
        cmd_uefi = common_qemu + ["-bios", ovmf_path, "-cdrom", iso_test]
        log_uefi = os.path.join(args.log_dir, "qemu_uefi_boot.log")
        if run_qemu_test(f"UEFI Boot Smoke Test ({os.path.basename(ovmf_path)})", cmd_uefi, markers_boot, 33, 15, log_uefi):
            tests_passed += 1
        else:
            tests_failed += 1
    else:
        print("Running test: UEFI Boot Smoke Test ... NOT RUN (OVMF firmware not found)")
        tests_skipped += 1

    # Test 3: Panic Diagnostic Smoke Test (if panic ISO was built)
    if os.path.isfile(iso_panic):
        cmd_panic = common_qemu + ["-cdrom", iso_panic]
        log_panic = os.path.join(args.log_dir, "qemu_panic_test.log")
        markers_panic = [
            "[ataos] AtaOS 0.1.0",
            "*** KERNEL PANIC ***",
            "deliberate test panic (ATAOS_TEST_PANIC)",
        ]
        if run_qemu_test("Panic Handler Smoke Test", cmd_panic, markers_panic, 35, 10, log_panic):
            tests_passed += 1
        else:
            tests_failed += 1
    else:
        print(f"note: panic ISO not found at {iso_panic}, skipping panic test")
        tests_skipped += 1

    # Test 4: CPU Exception Smoke Test (if fault ISO was built)
    if os.path.isfile(iso_fault):
        cmd_fault = common_qemu + ["-cdrom", iso_fault]
        log_fault = os.path.join(args.log_dir, "qemu_fault_test.log")
        markers_fault = [
            "[ataos] AtaOS 0.1.0",
            "*** CPU EXCEPTION ***",
            "Vector: 6 (Invalid Opcode (#UD))",
            "System halted.",
        ]
        if run_qemu_test("CPU Exception Smoke Test (#UD)", cmd_fault, markers_fault, 35, 10, log_fault):
            tests_passed += 1
        else:
            tests_failed += 1
    else:
        print(f"note: fault ISO not found at {iso_fault}, skipping exception test")
        tests_skipped += 1

    print("\n=== QEMU Test Results ===")
    print(f"Passed:  {tests_passed}")
    print(f"Failed:  {tests_failed}")
    print(f"Skipped: {tests_skipped}")

    if tests_failed > 0:
        return 1
    return 0

if __name__ == "__main__":
    sys.exit(main())
