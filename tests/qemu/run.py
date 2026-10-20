#!/usr/bin/env python3
"""
AtaOS Headless QEMU Smoke Test Runner (ADR-0014).
Runs BIOS and UEFI tests, asserts serial output markers, and validates ISA debug-exit codes.
"""

import argparse
import os
import signal
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

def kill_process_group(proc):
    """Kill QEMU and any helper it started, including on timeout."""
    if proc.poll() is not None:
        return
    try:
        if os.name == "posix":
            os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
        else:
            proc.kill()
    except (ProcessLookupError, PermissionError):
        try:
            proc.kill()
        except ProcessLookupError:
            pass
    try:
        proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        pass


def print_log_tail(path, lines=40):
    try:
        with open(path, "r", errors="replace") as f:
            content = f.read().splitlines()
    except OSError as exc:
        print(f"  Could not read serial log: {exc}")
        return
    tail = content[-lines:]
    print(f"--- Last {len(tail)} serial lines ({path}) ---")
    for line in tail:
        print(line)
    print("------------------------------------------")


def run_trace(qemu_cmd, trace_log, timeout_sec=5):
    """Optional diagnostic re-run with interrupt/CPU-reset tracing."""
    trace_cmd = list(qemu_cmd)
    # A trace run must keep QEMU alive after a triple fault so the log is
    # complete; the timeout below terminates it.
    if "-no-shutdown" not in trace_cmd:
        trace_cmd.append("-no-shutdown")
    trace_cmd += ["-d", "int,cpu_reset", "-D", trace_log]
    try:
        # QEMU writes the -d trace through -D. Keep stdout away from that file
        # so serial output cannot truncate or interleave with the trace.
        proc = subprocess.Popen(
            trace_cmd,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            start_new_session=(os.name == "posix"),
        )
        try:
            proc.wait(timeout=timeout_sec)
        except subprocess.TimeoutExpired:
            kill_process_group(proc)
    except OSError as exc:
        print(f"  Trace re-run failed: {exc}")
        return
    print(f"  Trace log captured in: {trace_log}")


def run_qemu_test(name, qemu_cmd, expected_markers, expected_exit, timeout_sec, log_path,
                  trace_on_timeout=False, trace_log=None):
    print(f"Running test: {name} ... ", end="", flush=True)
    timed_out = False
    start_time = time.time()
    with open(log_path, "w") as log_file:
        proc = subprocess.Popen(
            qemu_cmd,
            stdout=log_file,
            stderr=subprocess.STDOUT,
            start_new_session=(os.name == "posix"),
        )
        try:
            exit_code = proc.wait(timeout=timeout_sec)
        except subprocess.TimeoutExpired:
            timed_out = True
            kill_process_group(proc)
            log_file.flush()
            exit_code = None
    duration = time.time() - start_time

    if timed_out:
        print("FAILED (Timeout)")
        print(f"  Log output captured in: {log_path}")
        print_log_tail(log_path)
        if trace_on_timeout and trace_log:
            run_trace(qemu_cmd, trace_log)
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
    parser.add_argument("--trace-on-timeout", action="store_true",
                        help="Re-run a timed-out test with -d int,cpu_reset diagnostics")
    parser.add_argument("--require-all", action="store_true",
                        help="Fail when a test image or firmware is missing instead of skipping")
    args = parser.parse_args()

    os.makedirs(args.log_dir, exist_ok=True)

    iso_test = os.path.join(args.root, "build", f"{args.mode}-test", "ataos.iso")
    iso_panic = os.path.join(args.root, "build", f"{args.mode}-testpanic", "ataos.iso")

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
    if run_qemu_test("BIOS Boot Smoke Test", cmd_bios, markers_boot, 33, 10, log_bios,
                     args.trace_on_timeout,
                     os.path.join(args.log_dir, "qemu_bios_boot-trace.log")):
        tests_passed += 1
    else:
        tests_failed += 1

    # Test 2: UEFI Boot Smoke Test
    ovmf_path = find_ovmf()
    if ovmf_path:
        cmd_uefi = common_qemu + ["-bios", ovmf_path, "-cdrom", iso_test]
        log_uefi = os.path.join(args.log_dir, "qemu_uefi_boot.log")
        if run_qemu_test(f"UEFI Boot Smoke Test ({os.path.basename(ovmf_path)})", cmd_uefi,
                         markers_boot, 33, 15, log_uefi, args.trace_on_timeout,
                         os.path.join(args.log_dir, "qemu_uefi_boot-trace.log")):
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
        if run_qemu_test("Panic Handler Smoke Test", cmd_panic, markers_panic, 35, 10,
                         log_panic, args.trace_on_timeout,
                         os.path.join(args.log_dir, "qemu_panic_test-trace.log")):
            tests_passed += 1
        else:
            tests_failed += 1
    else:
        print(f"note: panic ISO not found at {iso_panic}, skipping panic test")
        tests_skipped += 1

    # Test 4: CPU Exception Smoke Tests (deliberate fault injection)
    fault_tests = [
        ("de", "Divide Error (#DE)", ["Vector: 0 (Divide Error (#DE))"]),
        ("ud", "Invalid Opcode (#UD)", ["Vector: 6 (Invalid Opcode (#UD))"]),
        ("gp", "General Protection Fault (#GP)", ["Vector: 13 (General Protection Fault (#GP))"]),
        ("pf", "Page Fault (#PF)",
         ["Vector: 14 (Page Fault (#PF))", "CR2: 0x0000000000000100",
          "PF: CR2=0x0000000000000100 origin=kernel access=write cause=not-present fetch=data"]),
        ("pfwp", "Page Fault (#PF write-protect)",
         ["Vector: 14 (Page Fault (#PF))", "origin=kernel access=write cause=protection",
          "hint=kernel write to a read-only/W^X-protected page"]),
        ("pfnx", "Page Fault (#PF NX)",
         ["Vector: 14 (Page Fault (#PF))", "origin=kernel access=fetch cause=protection",
          "hint=instruction fetch from an NX page"]),
        ("df", "Double Fault (#DF via IST)",
         ["Vector: 8 (Double Fault (#DF))", "DF: delivered on IST1"]),
        ("stackovf", "Kernel stack guard (#DF via IST1)",
         ["Vector: 8 (Double Fault (#DF))", "DF: delivered on IST1",
          "System halted."]),
    ]

    for flavor, desc, extra_markers in fault_tests:
        iso_fault_flavor = os.path.join(args.root, "build", f"{args.mode}-testfault-{flavor}", "ataos.iso")
        if os.path.isfile(iso_fault_flavor):
            cmd_fault = common_qemu + ["-cdrom", iso_fault_flavor]
            log_fault = os.path.join(args.log_dir, f"qemu_fault_{flavor}.log")
            markers_fault = [
                "[ataos] AtaOS 0.1.0",
                "*** CPU EXCEPTION ***",
                "System halted.",
            ] + extra_markers
            if run_qemu_test(f"CPU Exception Test: {desc}", cmd_fault, markers_fault, 35, 10,
                             log_fault, args.trace_on_timeout,
                             os.path.join(args.log_dir, f"qemu_fault_{flavor}-trace.log")):
                tests_passed += 1
            else:
                tests_failed += 1
        else:
            print(f"note: fault ISO not found at {iso_fault_flavor}, skipping {desc} test")
            tests_skipped += 1

    print("\n=== QEMU Test Results ===")
    print(f"Passed:  {tests_passed}")
    print(f"Failed:  {tests_failed}")
    print(f"Skipped: {tests_skipped}")

    if tests_failed > 0:
        return 1
    if args.require_all and tests_skipped > 0:
        print("error: --require-all was requested but some tests were skipped")
        return 1
    return 0

if __name__ == "__main__":
    sys.exit(main())
