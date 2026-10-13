# AtaOS Compatibility Matrix

This document tracks verified compatibility across virtualization platforms and firmware types, per **QR-005** and **ADR-0014**.

---

## 1. Emulator and Hypervisor Support Matrix

| Environment | Firmware Mode | Device Configuration | Boot Status | Tests Executed | Milestone Verified | Notes |
|---|---|---|---|---|---|---|
| **QEMU (v8.2.2)** | BIOS | `-machine q35 -m 256M -cpu qemu64` | **VERIFIED** | Automated smoke test (exit 33) | M1 | Serial banner & memmap verified |
| **QEMU (v8.2.2)** | UEFI (OVMF) | `-machine q35 -m 256M -bios OVMF.fd` | **VERIFIED** | Automated smoke test (exit 33) | M1 | OVMF x64 firmware boot verified |
| **VirtualBox 7.x** | BIOS | 256MB RAM, 1 CPU, IDE CD-ROM, COM1 file | **VERIFIED** | Headless automated verification | M1 | Verified via `scripts/vbox-run.sh` |
| **VirtualBox 7.x** | UEFI | 256MB RAM, 1 CPU, IDE CD-ROM, COM1 file | Planned | Manual checklist | M2 | Scheduled for M2 verification |
| **Bare Metal** | UEFI / BIOS | x86_64 PC (Intel / AMD) | Untested | None | Future | Deferred until disk & keyboard drivers |

---

## 2. Hardware Emulation Differences & Mitigation

### 1. Serial Port (COM1 / 16550 UART)
- **QEMU:** Emulates 16550A UART instantly with virtual zero-delay transmit FIFO.
- **VirtualBox:** Stricter UART timing; requires DTR/RTS and proper divisor configuration (Divisor 1 = 115200 baud).
- **Mitigation:** `arch/x86_64/cpu/serial.c` fully initializes line control register, baud divisor, FIFO control, and modem control registers according to the standard 16550 specification.

### 2. Memory Map Reporting
- Limine normalizes memory map entries across both UEFI (`GetMemoryMap`) and BIOS (INT 15h E820h).
- AtaOS treats all memory regions non-destructively: only `LIMINE_MEMMAP_USABLE` is eligible for physical frame allocation in M3.

### 3. Test-Only Debug Exit
- QEMU supports the `isa-debug-exit` peripheral on port `0xf4`.
- VirtualBox does not emulate this port by default.
- **Policy:** The test-only exit device is strictly gated by `-DATAOS_TEST_BUILD` (`TEST=1`) and is excluded from standard release builds (`TEST=0`).
