# AtaOS — A Modular x86_64 Operating System

AtaOS is an educational, architecturally sound x86_64 operating system built from scratch, booted via the modern Limine bootloader protocol, and targeting bare-metal, QEMU, and VirtualBox.

---

## Current Status: Milestone M1 (Repository & Boot Foundation)

- **Architecture:** Higher-half x86_64 kernel (`-mcmodel=kernel`, base `0xffffffff80000000`).
- **Bootloader:** Pinned Limine v12.9.3 (Protocol Base Revision 6).
- **Entry Mode:** Long Mode (64-bit), Ring 0.
- **Diagnostics:** Early 16550 UART (COM1) serial driver, formatted `klog` logging, kernel panic with stack/caller info.
- **Verification:** Unit tests on host (AddressSanitizer + UBSan), automated headless QEMU smoke tests (BIOS & UEFI), reproducible builds.

---

## Quick Start (WSL2 / Linux)

### 1. Prerequisites
Ensure the following tools are installed on your Linux / WSL2 system:
- `clang`, `lld`, `make`, `python3`
- `xorriso`, `mtools` (for ISO generation)
- `qemu-system-x86_64`, `ovmf` (for QEMU emulation)

Verify your environment:
```bash
make env-check
```

### 2. Build the Kernel and ISO
```bash
make           # Builds build/debug/ataos.elf
make iso       # Builds build/debug/ataos.iso
```

### 3. Run in QEMU
```bash
make run       # Boots ISO in QEMU (legacy BIOS) with serial on stdio
make run-uefi  # Boots ISO in QEMU with UEFI (OVMF)
make run-debug # Freezes at reset vector, opens GDB stub on :1234
```
*(Press `Ctrl-A` then `X` to exit QEMU).*

### 4. Run Test Suite
```bash
make test-host     # Runs string, fmt, bits unit tests with ASan/UBSan
make test-scripts  # Runs commit validator and script tests
make test-qemu     # Runs headless QEMU boot smoke tests
make repro-check   # Verifies bit-for-bit reproducible ELF build
make test          # Runs all tests
```

---

## Architecture and Documentation

The architecture is systematically documented in `docs/`:

- [ARCHITECTURE.md](file:///c:/Users/cagri/OneDrive/Desktop/kuiz/gitHUB/cagOS/AtaOS/AtaOS/docs/ARCHITECTURE.md) — System layers, contracts, and module boundaries.
- [REQUIREMENTS.md](file:///c:/Users/cagri/OneDrive/Desktop/kuiz/gitHUB/cagOS/AtaOS/AtaOS/docs/REQUIREMENTS.md) — Functional, architectural, and quality requirements.
- [MEMORY_LAYOUT.md](file:///c:/Users/cagri/OneDrive/Desktop/kuiz/gitHUB/cagOS/AtaOS/AtaOS/docs/MEMORY_LAYOUT.md) — Virtual address spaces and physical memory ownership.
- [BOOT.md](file:///c:/Users/cagri/OneDrive/Desktop/kuiz/gitHUB/cagOS/AtaOS/AtaOS/docs/BOOT.md) — Limine protocol integration and initialization phases.
- [SYSCALL_ABI.md](file:///c:/Users/cagri/OneDrive/Desktop/kuiz/gitHUB/cagOS/AtaOS/AtaOS/docs/SYSCALL_ABI.md) — System call interface draft (M5).
- [PROCESS_MODEL.md](file:///c:/Users/cagri/OneDrive/Desktop/kuiz/gitHUB/cagOS/AtaOS/AtaOS/docs/PROCESS_MODEL.md) — Processes, threads, and scheduler design (M6).
- [FILESYSTEM.md](file:///c:/Users/cagri/OneDrive/Desktop/kuiz/gitHUB/cagOS/AtaOS/AtaOS/docs/FILESYSTEM.md) — VFS and initial filesystem architecture (M8).
- [SECURITY.md](file:///c:/Users/cagri/OneDrive/Desktop/kuiz/gitHUB/cagOS/AtaOS/AtaOS/docs/SECURITY.md) — Trust boundaries, W^X, and pointer validation.
- [ROADMAP.md](file:///c:/Users/cagri/OneDrive/Desktop/kuiz/gitHUB/cagOS/AtaOS/AtaOS/ROADMAP.md) — 60-day phased roadmap (Milestones M1–M12).
- [Architecture Decision Records (ADRs)](file:///c:/Users/cagri/OneDrive/Desktop/kuiz/gitHUB/cagOS/AtaOS/AtaOS/docs/decisions/) — ADR-0001 through ADR-0016.

---

## Directory Structure

```text
AtaOS/
├── arch/x86_64/         # Architecture-specific x86_64 CPU, serial, and boot entry
├── boot/                # Bootloader configurations (limine.conf)
├── docs/                # Architecture, design documents, and ADRs
├── include/kernel/      # Core kernel public interface headers
├── kernel/              # Architecture-independent kernel subsystems
│   ├── core/            # Entry point, logging, panic
│   └── lib/             # Freestanding C library (string, fmt)
├── scripts/             # Build, ISO generation, and verification scripts
├── tests/               # Host unit tests and headless QEMU test runner
├── Makefile             # Top-level build orchestration
└── toolchain.mk         # Freestanding compiler and flag definitions
```
