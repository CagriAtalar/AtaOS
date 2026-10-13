# AtaOS Testing Strategy and Verification Guide

This document defines the verification strategy for AtaOS across all development phases, per **ADR-0014**. Testing is strictly layered, deterministic, and traceable to requirement identifiers in [REQUIREMENTS.md](file:///c:/Users/cagri/OneDrive/Desktop/kuiz/gitHUB/cagOS/AtaOS/AtaOS/docs/REQUIREMENTS.md).

---

## 1. Testing Philosophy and Rules

1. **Truthfulness:** Never claim that code compiles, boots, or passes a test unless verified by actual execution. Mark untested configurations explicitly as `NOT RUN` with reasons.
2. **Layered Verification:**
   - **Host-side unit tests:** Pure algorithms, string/memory operations, formatting, bitmap manipulation, and data structures. Tested under AddressSanitizer (ASan) and UndefinedBehaviorSanitizer (UBSan).
   - **Kernel QEMU smoke tests:** Automated headless boot tests asserting serial banners, memory map sanity, panic output, and deterministic ISA debug-exit codes.
   - **VirtualBox verification:** Conservative, non-destructive manual/semi-automated test protocols using isolated VM configurations.
   - **Script and toolchain tests:** Automated checks for commit message format, dependency boundaries, reproducible builds, and Limine hash integrity.
3. **No Placeholders:** Tests must assert meaningful invariants. Empty or tautological tests are prohibited.
4. **Regression Protection:** Every bug fix must be accompanied by a regression test or reproduction case where practical.

---

## 2. Test Layer 1: Host-Side Unit Tests (`tests/host/`)

Host tests execute freestanding kernel library code compiled for the host OS (`-DATAOS_HOST_TEST`), linked with AddressSanitizer and UBSan when supported by the host compiler.

### Covered Components in M1:
- `kernel/lib/string.c`: `memcpy`, `memmove`, `memset`, `memcmp`, `strlen`, `strnlen`, `strncmp`, `strlcpy`.
- `kernel/lib/fmt.c`: `snprintf`, `vsnprintf` (`%s`, `%c`, `%d`, `%u`, `%x`, `%lx`, `%lu`, `%p`, `%zu`, `%%`, padding, width).
- `include/kernel/bits.h`: Alignment macros (`ALIGN_UP`, `ALIGN_DOWN`, `IS_ALIGNED`), bit manipulation (`DIV_ROUND_UP`).

### Running Host Tests:
```bash
make test-host
```
Host tests exit with status `0` on success and non-zero on failure. Under ASan/UBSan, any memory corruption, buffer overflow, or undefined behavior immediately halts execution with a detailed diagnostic.

---

## 3. Test Layer 2: Headless QEMU Smoke Tests (`tests/qemu/run.py`)

AtaOS provides a Python-based headless test runner for QEMU.

### Test Matrix for M1:
1. **BIOS Boot Smoke Test:**
   - Command: `qemu-system-x86_64 -machine q35 -m 256M -cpu qemu64 -smp 1 -cdrom build/debug_test/ataos.iso -display none -serial stdio -device isa-debug-exit,iobase=0xf4,iosize=0x04`
   - Expected Output:
     - `[ataos] === AtaOS Kernel v0.1.0-dev ===`
     - `[ataos] bootloader: Limine`
     - `[ataos] HHDM offset: 0xffff800000000000`
     - `[ataos] boot complete, entering halt loop`
   - Exit Code: `33` (`0x21` via QEMU debug-exit port `0xf4`, indicating pass).
2. **UEFI Boot Smoke Test:**
   - Same as above with `-bios /usr/share/OVMF/OVMF.fd` or `/usr/share/ovmf/OVMF.fd`.
   - If OVMF firmware is missing, the test reports `NOT RUN (OVMF not found)` and exits non-fatally with notification.
3. **Panic Diagnostic Smoke Test:**
   - Built with `TEST_PANIC=1`.
   - Asserts that the kernel prints `KERNEL PANIC:`, file, line number, disables interrupts, and exits via port `0xf4` with code `35` (`0x23`).

### Running QEMU Tests:
```bash
make test-qemu
```

---

## 4. Test Layer 3: Script and Toolchain Verification

Automated scripts verify repository policies before commits or in CI:

1. **Commit Message Validator (`tests/scripts/test-commit-msg.sh`):**
   - Asserts that valid Conventional Commit formats (`feat(boot): ...`) pass.
   - Asserts that invalid formats (missing scope, capitalized summary, trailing period, header > 72 chars) are rejected with clear diagnostics.
2. **Limine Checksum Test (`tests/scripts/test-fetch-limine.sh`):**
   - Asserts that downloaded binary tarball matches pinned SHA-256 (`9f42fe9ea...`).
3. **Dependency Boundary Check (`scripts/check-deps.sh`):**
   - Scans kernel sources to ensure no architecture-independent code includes `arch/` private headers or host `<stdio.h>`.
4. **Reproducible Build Check (`scripts/repro-check.sh`):**
   - Compiles kernel twice into separate build directories and asserts identical SHA-256 hashes of the resulting ELF.

### Running Script Tests:
```bash
make test-scripts
make repro-check
```

---

## 5. Test Layer 4: Oracle VirtualBox Verification Procedure

VirtualBox testing verifies hardware compatibility on a secondary hypervisor (per **QR-005**).

### Safety Invariants:
- **Zero Destruction:** Scripts must never delete, modify, or overwrite any existing user VM.
- **Prefix Isolation:** All temporary VMs must use the prefix `ataos-verify-` (e.g. `ataos-verify-m1`).
- **Headless Execution:** Designed to execute non-interactively where `VBoxManage` is available.

### Manual Verification Checklist (Windows Host / VirtualBox 7.x):
1. **Create VM:**
   ```powershell
   & "C:\Program Files\Oracle\VirtualBox\VBoxManage.exe" createvm --name "ataos-verify-m1" --ostype "Other_64" --register
   & "C:\Program Files\Oracle\VirtualBox\VBoxManage.exe" modifyvm "ataos-verify-m1" --memory 256 --cpus 1 --firmware bios
   & "C:\Program Files\Oracle\VirtualBox\VBoxManage.exe" storagectl "ataos-verify-m1" --name "IDE" --add ide
   & "C:\Program Files\Oracle\VirtualBox\VBoxManage.exe" storageattach "ataos-verify-m1" --storagectl "IDE" --port 0 --device 0 --type dvddrive --medium "build\debug\ataos.iso"
   & "C:\Program Files\Oracle\VirtualBox\VBoxManage.exe" modifyvm "ataos-verify-m1" --uart1 0x3F8 4 --uartmode1 file "vbox_serial.log"
   ```
2. **Start VM:**
   ```powershell
   & "C:\Program Files\Oracle\VirtualBox\VBoxManage.exe" startvm "ataos-verify-m1" --type headless
   ```
3. **Inspect Output:**
   Inspect `vbox_serial.log` for the boot banner:
   ```text
   [ataos] === AtaOS Kernel v0.1.0-dev ===
   [ataos] boot complete, entering halt loop
   ```
4. **Clean up:**
   ```powershell
   & "C:\Program Files\Oracle\VirtualBox\VBoxManage.exe" controlvm "ataos-verify-m1" poweroff
   & "C:\Program Files\Oracle\VirtualBox\VBoxManage.exe" unregistervm "ataos-verify-m1" --delete
   ```

---

## 6. Complete Test Command Reference

| Command | Target | Requirements Verified |
|---|---|---|
| `make test-host` | Pure unit tests with ASan/UBSan | QR-002, QR-003 |
| `make test-scripts` | Script & commit validator tests | QR-001, ADR-0015 |
| `make test-qemu` | Headless QEMU BIOS/UEFI smoke tests | FR-001, QR-003 |
| `make repro-check` | Bit-for-bit ELF reproducibility check | QR-001 |
| `make test` | Runs host, script, and QEMU tests | Complete M1 suite |
