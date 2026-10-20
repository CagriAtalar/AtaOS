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

### Covered Components in M4:
- `kernel/lib/string.c`: `memcpy`, `memmove`, `memset`, `memcmp`, `strlen`, `strnlen`, `strncmp`, `strlcpy`.
- `kernel/lib/fmt.c`: `snprintf`, `vsnprintf` (`%s`, `%c`, `%d`, `%u`, `%x`, `%lx`, `%lu`, `%p`, `%zu`, `%%`, padding, width).
- `include/kernel/bits.h`: alignment macros (`ALIGN_UP`, `ALIGN_DOWN`, `IS_ALIGNED`), bit manipulation (`DIV_ROUND_UP`).
- `kernel/mm/bitmap.c` and `kernel/mm/heap.c`: frame bitmap, split/coalesce, overflow rejection.
- `kernel/mm/mm_internal.c`: canonical PML4 bases, 4 KiB/2 MiB/1 GiB exact translation, fallible table allocation and rollback.
- `arch/x86_64/mm/paging_encode.c`: level-aware PAT/PS decoding, W^X flags, permission-preserving re-protection.
- `kernel/mm/vmm_validate.c`: range/flag/overflow validation shared with `vmm_map`/`aspace_map`.
- `kernel/mm/aspace.c`: private user half, shared kernel half, mapping lifecycle and PMM-counter return.
- `arch/x86_64/interrupt/page_fault.c`: every documented #PF error-code bit and diagnostic summary.

### Running Host Tests:
```bash
make test-host
```
Host tests exit with status `0` on success and non-zero on failure. Under ASan/UBSan, any memory corruption, buffer overflow, or undefined behavior immediately halts execution with a detailed diagnostic. The M4 suite contains 40 tests; the expected summary is `Total: 40, Passed: 40, Failed: 0`.

---

## 3. Test Layer 2: Headless QEMU Smoke Tests (`tests/qemu/run.py`)

AtaOS provides a Python-based headless test runner for QEMU.

### Test Matrix for M4 (11 tests):
1. **BIOS Boot Smoke Test:** asserts the AtaOS/Limine/HHDM/boot-complete banners and exit code `33`.
2. **UEFI Boot Smoke Test:** same markers with OVMF; reports `NOT RUN` if firmware is absent unless `--require-all` is set.
3. **Panic Diagnostic Smoke Test:** `TEST_PANIC=1`; asserts a structured panic and exit code `35`.
4. **#DE, #UD, #GP:** deliberate fault injection; asserts the vector dump, halt, and exit code `35`.
5. **#PF (unmapped):** asserts `CR2=0x100`, `origin=kernel access=write cause=not-present`, halt, exit `35`.
6. **#PF write-protect:** writes RX kernel text; asserts `access=write cause=protection` and the W^X hint.
7. **#PF NX:** fetches from NX data; asserts `access=fetch cause=protection` and the NX hint.
8. **#DF via IST1:** zero-RSP fault path; asserts the #DF vector and `DF: delivered on IST1`.
9. **Kernel stack guard:** `stackovf` pushes into the unmapped boot-stack guard; the failed #PF frame push escalates to #DF on IST1. Asserts `CR2` in the guard page, the IST1 line, and a clean halt.

All exception tests assert `*** CPU EXCEPTION ***` and `System halted.` in addition to the vector-specific markers.

### Running QEMU Tests:
```bash
make test-qemu
# CI mode: fail instead of skipping missing images/firmware
make test-qemu QEMU_RUN_FLAGS=--require-all
# Diagnostic mode: re-run a timed-out test with -d int,cpu_reset -no-shutdown
make test-qemu QEMU_RUN_FLAGS=--trace-on-timeout
```

The runner isolates QEMU in its own process group, kills the group on timeout, prints the final serial lines, and keeps the `-D` trace file separate from serial output. See [DEBUGGING.md](DEBUGGING.md) §3.

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
| `make test-qemu` | Headless QEMU BIOS/UEFI, panic and 8 exception tests | FR-001, QR-003 |
| `make repro-check` | Bit-for-bit ELF reproducibility check | QR-001 |
| `make test` | Runs host, script, and QEMU tests | Complete M4 suite |

GitHub Actions (`.github/workflows/ci.yml`) runs the same host, script, lint,
reproducibility and QEMU layers on Ubuntu, with `QEMU_RUN_FLAGS=--require-all` so
a missing UEFI image is a failure rather than a silent skip. CI was not executed
locally in this session; the workflow file is `NOT RUN` evidence until a push or
manual workflow dispatch runs it.
