# AtaOS Debugging and Diagnostics Guide

This document describes how to debug the AtaOS kernel, inspect registers, connect GDB, and diagnose boot issues and panics.

---

## 1. Early Serial Output (COM1)

AtaOS initializes the 16550 UART on COM1 (`0x3F8`) immediately upon kernel entry (in `arch/x86_64/cpu/serial.c`).

Serial output is the primary diagnostic channel for all early boot stages:
- **Port:** `0x3F8` (COM1), 115200 baud, 8 data bits, 1 stop bit, no parity, FIFO enabled.
- **Log Levels:** `PANIC`, `ERROR`, `WARN`, `INFO`, `DEBUG`.
- In QEMU, serial is mapped directly to standard output or log file:
  ```bash
  # Run with interactive serial in terminal (press Ctrl-A then X to quit)
  make run
  ```

---

## 2. Remote Kernel Debugging with GDB

The unstripped kernel ELF (`build/<config>/ataos.elf`) retains full DWARF debug symbols and frame pointers (`-g -fno-omit-frame-pointer`).

### Starting QEMU with GDB Stub
```bash
make run-debug
```
This launches QEMU with `-s -S`, which freezes the CPU at the reset vector and listens for a GDB connection on TCP port `1234`.

### Connecting with GDB
In a second terminal:
```bash
gdb build/debug/ataos.elf \
    -ex "target remote :1234" \
    -ex "break kmain" \
    -ex "continue"
```

### Useful GDB Commands for AtaOS:
- `layout src`: View C source synchronized with execution.
- `info registers`: Dump CPU general-purpose registers (RAX, RBX, RCX, RDX, RSI, RDI, RSP, RBP, RIP, R8-R15).
- `print/x *(struct limine_bootloader_info_response*)boot_info.bootloader`: Inspect Limine bootloader response struct.
- `x/16gx $rsp`: Inspect stack frames.
- `backtrace` or `bt`: Show call stack.
- `monitor info mem`: Inspect QEMU virtual-to-physical memory mappings.
- `monitor info registers`: Inspect CPU control registers (CR0, CR2, CR3, CR4, EFER).

---

## 3. Diagnosing Triple Faults and CPU Panics

If QEMU suddenly restarts or exits unexpectedly:

### 1. Enable QEMU Register and Interrupt Tracing
Run QEMU with `-d int,cpu_reset -D qemu.log -no-reboot`:
```bash
qemu-system-x86_64 -machine q35 -m 256M -cpu qemu64 -smp 1 -cdrom build/debug/ataos.iso \
    -display none -serial stdio -d int,cpu_reset -D qemu_trace.log -no-reboot
```
Inspect `qemu_trace.log`:
- Check the last `RIP` before reset.
- Check `CR2` (contains the faulting linear address during a Page Fault `#PF`).
- Check `CR3` (ensures page directory base points to a valid physical PML4).

### 2. Kernel Panic Diagnostics
When a kernel invariant fails or an explicit `panic()` is triggered:
- The panic subsystem disables interrupts (`cli`).
- Prints `KERNEL PANIC: <message> at <file>:<line>`.
- In test builds (`TEST=1`), signals QEMU `isa-debug-exit` port `0xf4` with code `35`.
- In normal builds, enters a permanent halt loop (`for (;;) { hlt(); }`).

### 3. QEMU Runner Timeouts and `-no-shutdown` Traces

The headless runner (`tests/qemu/run.py`) isolates each QEMU process in its own
process group, kills the whole group on timeout, prints the last serial lines,
and reports `FAILED (Timeout)` instead of hanging. To capture a trace for a
timeout, run:

```bash
make test-qemu QEMU_RUN_FLAGS=--trace-on-timeout
```

The runner re-runs only the timed-out test with `-d int,cpu_reset -D
build/logs/<test>-trace.log` and adds `-no-shutdown`, so QEMU remains alive
after a triple fault and the complete interrupt/reset history reaches the trace
file. Serial output and the `-D` trace use separate sinks; do not point both at
the same file. `-no-shutdown` is diagnostic only: without it, QEMU exits on a
triple fault and the trace can end at the reset.

Pre-flight failures before the CR3 load are reported as `vmm: pre-flight ...`
panics with the failing spec name; a guard page that the bootloader mapped with
a large leaf is rejected rather than silently split.

### 4. Decoding Page Faults

Every #PF report includes the raw error code and:

```text
PF: CR2=0x... origin=kernel|user access=read|write|fetch cause=protection|not-present fetch=data|instruction
PF: present=0|1 write=0|1 user=0|1 reserved=... pk=... sgx=... class=supervisor-fatal|user-demand
PF: hint=...
```

`origin=kernel` is fatal in M4. `cause=not-present` on a kernel address is a
missing translation (not demand paging, which does not exist yet).
`cause=protection` with `access=write` is a W^X/read-only violation and with
`access=fetch` is an NX violation.

---

## 5. Framebuffer and Screen Diagnostics

If the kernel hangs before serial is initialized:
- Limine provides a linear framebuffer when requested (`limine_framebuffer_request`).
- In M1, framebuffer presence is detected and logged to serial.
- In M2/M3, early graphical panic drawing will be added as a fallback console.
