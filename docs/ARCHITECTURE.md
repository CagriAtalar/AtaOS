# AtaOS Architecture

> **Authoritative document.** Every important decision is explained once, here
> or in one linked document/ADR. If another document disagrees with this one,
> this one wins and the other is a bug. Detail documents:
> [MEMORY_LAYOUT](MEMORY_LAYOUT.md) · [BOOT](BOOT.md) ·
> [SYSCALL_ABI](SYSCALL_ABI.md) · [PROCESS_MODEL](PROCESS_MODEL.md) ·
> [FILESYSTEM](FILESYSTEM.md) · [SECURITY](SECURITY.md) ·
> [TESTING](TESTING.md) · [ADRs](decisions/).

## 0. Status labels

| Label | Meaning |
|---|---|
| `IMPLEMENTED` | Code exists in the repository. |
| `TESTED` | Specific tests were executed (named in the report). |
| `VERIFIED` | All declared acceptance criteria for the subsystem passed. |
| `PLANNED` | Design exists; no implementation. |
| `BLOCKED` | Progress depends on an unresolved issue. |

A design in this document is **not** evidence that a feature exists. Section 14
is the single status table.

## 1. Overview

AtaOS is a monolithic-style x86_64 kernel with explicit internal module
boundaries, booted by Limine into 64-bit long mode in the higher half. It is
single-CPU in its first 60 days. Ring 3 programs are separate ELF64
executables using a documented syscall ABI.

```
 +--------------------------------------------------------------+
 |  user/   init · shell · programs · libc  (Ring 3, ELF64)     |
 +----------------------------^---------------------------------+
                              | include/abi/  (only shared header dir)
 +----------------------------+---------------------------------+
 |  kernel/syscall   argument validation, dispatch              |
 +--------------------------------------------------------------+
 |  kernel/proc  kernel/sched  kernel/fs (VFS)  kernel/drivers  |
 +--------------------------------------------------------------+
 |  kernel/mm  (PMM · VMM policy · heap)     kernel/core (init, |
 |                                           log, panic)        |
 +--------------------------------------------------------------+
 |  kernel/lib  freestanding helpers (string, fmt, bits)        |
 +----------------------------^---------------------------------+
                              | include/kernel/arch.h  (narrow)
 +----------------------------+---------------------------------+
 |  arch/x86_64   boot glue · cpu · interrupt · mm · context    |
 +--------------------------------------------------------------+
 |  Limine  ->  firmware (BIOS or UEFI)  ->  hardware / VM      |
 +--------------------------------------------------------------+
```

## 2. Boot flow (target design)

```
Firmware -> Limine -> kernel entry (_start) -> kmain
   -> early init (serial, log, boot-info capture)       [M1, IMPLEMENTED]
   -> CPU init (GDT/TSS, IDT, exceptions)               [M2, PLANNED]
   -> memory init (PMM, VMM, heap)                      [M3-M4, PLANNED]
   -> syscall + Ring 3 entry                            [M5, PLANNED]
   -> timer + scheduler init                            [M6, PLANNED]
   -> initial user process (init)                       [M7-M9, PLANNED]
```

In M1 the boot path stops after the boot summary and a final marker
(`[ataos] boot complete`) and then halts safely. See [BOOT.md](BOOT.md).

## 3. Modules and responsibilities

| Module | Path | Owns | Must not |
|---|---|---|---|
| Kernel core | `kernel/core/` | Init order, lifecycle, panic/assert, logging, boot-info capture | Implement other subsystems |
| Freestanding lib | `kernel/lib/` | `mem*`/`str*`, formatter, bit/alignment helpers | Call into any other kernel module; use the heap; use hosted libc |
| Arch layer | `arch/x86_64/` | Entry asm, GDT/TSS, IDT, CR/MSR, context switch, page-table bit formats, TLB, per-CPU access, syscall entry, PIC/APIC/timer mechanisms, port I/O, early UART | Contain scheduling, VFS, process or allocation policy |
| Memory mgmt | `kernel/mm/` | Frame allocator, address-space objects, mapping policy, kmalloc | Touch page-table encodings (calls `arch_mm_*`) |
| Process | `kernel/proc/` | Process/thread identity, lifecycle, fd table ownership | Inspect allocator internals |
| Scheduler | `kernel/sched/` | Run queues, policy, block/wake | Know x86 context layout (calls `arch_context_switch`) |
| Syscall | `kernel/syscall/` | Dispatch, user-pointer validation, copy-in/out | Contain subsystem logic |
| FS / VFS | `kernel/fs/` | Path resolution, vnodes, open-file descriptions, mounts, filesystem drivers | Depend on a specific filesystem in the VFS core |
| Drivers | `kernel/drivers/` | Device-specific logic behind generic interfaces | Modify scheduler/process/FS/MM internals |
| ABI headers | `include/abi/` | Syscall numbers, errno values, user-visible structs | Contain kernel-private types |
| Public kernel headers | `include/kernel/` | Module interfaces | Be included by `user/` |
| User space | `user/` | `init`, shell, libc, programs | Link kernel symbols or include `include/kernel/` |

## 4. Dependency rules

Allowed direction (an arrow means "may include/call"):

```
user -> abi
syscall -> proc, sched, fs, mm(copy helpers), abi, core(log), lib
proc -> sched, mm, fs, core, lib
sched -> mm (stack alloc), core, lib, arch.h
fs -> core, lib, mm(heap)
drivers -> core, lib, mm(heap), arch.h (documented hooks only)
mm -> core, lib, arch.h
core -> lib, arch.h
lib -> (nothing)
arch/x86_64 -> lib, include/kernel/arch.h, include/kernel/{compiler,bits}.h
```

Prohibited:

1. Anything outside `arch/` using inline asm, port I/O, MSRs, or control
   registers.
2. `arch/` calling scheduler, VFS, process, or allocator **policy**. Arch code
   may call upward only through callbacks registered at init (e.g. the
   interrupt dispatch table), never by name.
3. `user/` including `include/kernel/`, `arch/`, or `kernel/`.
4. The kernel or `kernel/lib/` including hosted libc headers. Only the
   compiler-provided freestanding headers (`stddef.h`, `stdint.h`, `stdbool.h`,
   `stdarg.h`, `stdalign.h`, `limits.h`) are allowed.
5. The scheduler reading allocator or page-table internals.
6. A filesystem driver being named in `kernel/fs/vfs*.c`.
7. Build rules referencing generated files outside `build/`.
8. Cycles. A cycle is fixed by moving an interface (e.g. extracting a callback
   type), never by forward-declaring around it.

Exposed headers:

| Header | Provided by | May be included by |
|---|---|---|
| `include/kernel/arch.h` | arch layer interface | `kernel/*`, `arch/x86_64/*` |
| `include/kernel/{compiler,bits,string,fmt,log,panic,bootinfo}.h` | lib/core | all kernel modules and `arch/` (except `log.h`/`panic.h`/`bootinfo.h` in `arch/`: arch may use `panic.h` only) |
| `include/kernel/limine_boot.h` | core | `kernel/core` only (planned: PMM via `bootinfo.h` copies) |
| `arch/x86_64/include/arch/*.h` | arch | `arch/x86_64/*` only |
| `include/abi/*.h` | ABI | kernel and `user/` |

`scripts/check-deps.sh` enforces rules 1, 3, 4 mechanically. The other rules
are review items.

## 5. CPU execution model and state ownership

Target: x86_64 long mode, 4-level paging (48-bit canonical addresses), Ring 0
kernel / Ring 3 user, NX where available (EFER.NXE), SMEP/SMAP enabled when
CPUID reports them (M4).

| State | Provided by Limine at entry | Established by the kernel (milestone) |
|---|---|---|
| Mode | Long mode, paging on, interrupts disabled | — |
| GDT / segment regs | Valid Limine GDT (layout unspecified for kernel use) | Own GDT + TSS (M2) |
| IDT | **None** valid for kernel use | Own IDT (M2) |
| Page tables | Bootloader tables: kernel image mapped higher-half, HHDM mapped | Own PML4 derived from these mappings (M4) |
| Stack | Bootloader-provided | Own 16 KiB boot stack in `.bss` at `_start` (M1) |
| Control regs | CR0/CR4/EFER per protocol (see PROTOCOL.md "Machine State at Entry") | Explicit setup of WP, NXE, SMEP/SMAP, FPU policy (M2/M4) |
| FPU/SSE | Not guaranteed | **Policy:** kernel built without SSE/x87 (`-mno-sse -mno-80387`). User FPU state: lazy-or-eager save decided at M6 (ADR-0011 notes eager `fxsave` as default). |
| Per-CPU data | None | `struct percpu` reached via `GS` base (`swapgs` on user transitions) (M2/M5) |

The kernel does **not** assume that bootloader-provided state is what it
eventually needs; it re-establishes each item before depending on it.

Stacks: each thread has a kernel stack (16 KiB, guard page below, see
MEMORY_LAYOUT). Ring 3 → Ring 0 transitions switch to the thread's kernel stack
via `TSS.RSP0` (interrupts) or explicit swap (syscall). Double-fault, NMI and
machine-check use dedicated IST stacks. User stacks are user-owned memory
mapped in the process address space.

## 6. Memory architecture (summary; see MEMORY_LAYOUT.md)

Three distinct address kinds are never interchangeable:

| Kind | C type (planned) | Meaning |
|---|---|---|
| Physical address | `paddr_t` (`uint64_t` wrapper) | Position in RAM/MMIO |
| Virtual address | `vaddr_t` / typed pointers | Address in some address space |
| Kernel heap pointer | `void *` from `kmalloc` | Subset of kernel virtual space |

Conversions go through named helpers (`phys_to_hhdm`, `hhdm_to_phys`);
HHDM base is read from the bootloader, never assumed.

Planned components (all `PLANNED`):

* **PMM** (M3): bitmap frame allocator over Limine-usable regions; ADR-0006.
  Frames are the only unit of ownership; every frame has exactly one owner
  (free, kernel, page-table, process data). Counters: total/free/reserved.
* **VMM** (M4): 4-level tables; 4 KiB pages only initially (2 MiB pages only for
  HHDM if profiled); W^X enforced by `vmm_map` rejecting `W|X`; new user address
  spaces copy the kernel-half PML4 entries (shared) and start with an empty
  user half (private); ADR-0007.
* **Heap** (M3/M4): `kmalloc/kcalloc/kfree` over PMM→HHDM initially; header-based
  first-fit with size checks; later moves to its own virtual region. `krealloc`
  only when a caller needs it.
* **Page faults**: kernel-mode fault = panic with register dump; user-mode fault
  at a user address = terminate process (until demand paging exists).
* **User copy**: `copy_from_user`/`copy_to_user`/`strncpy_from_user` validate
  range (overflow-safe, `< USER_TOP`), then access via an exception-table-guarded
  copy so a fault returns `-EFAULT` instead of panicking.

## 7. Interrupts, exceptions, concurrency

### 7.1 Exceptions (M2)

* IDT with 256 entries; vectors 0–31 mapped to exception stubs that push a
  uniform frame (vector, error code (synthetic 0 if the CPU pushes none), then
  the CPU frame) and call `x86_exception_dispatch(struct trap_frame *)`.
* Fatal by default (#DE, #UD, #GP, #DF, #MC, kernel-mode #PF): print vector,
  error code, `RIP`, `RSP`, `CR2`, `CR3`, GPRs, then halt. Recoverable: user-mode
  faults terminate the offending process; kernel `copy_*_user` faults use the
  exception table.
* `#DF` and NMI use IST stacks so a corrupt kernel stack still yields a report.
* Preference: deterministic failure report over continuing with corrupted state.

### 7.2 Hardware interrupts and timer (M6; ADR-0008, ADR-0009)

* Controller: legacy 8259 PIC masked then **Local APIC + (I/O APIC later)** when
  needed; the APIC timer is the scheduler tick, calibrated against the PIT (or
  the Limine TSC-frequency feature when present); PIT is the fallback. HPET and
  the TSC-deadline mode are deferred. Calibration result and source are logged
  so QEMU/VirtualBox differences are observable.
* Acknowledgement: EOI is issued by the arch layer after the registered handler
  returns; spurious vector counted and ignored.
* Time base: monotonic `uint64_t` nanoseconds derived from TSC after calibration
  (fallback: tick count × period). Sleep/wakeup uses a timer queue (M6).

### 7.3 Single-CPU locking policy

* **Interrupt context** = exception, IRQ handlers. Handlers must not: sleep,
  take a sleeping lock, call `kmalloc`, perform lengthy I/O, or invoke the
  scheduler **except** the single documented hook `sched_tick()` which only sets
  a `need_resched` flag; the actual switch happens on interrupt return.
* **Spinlock** (M3+): `spin_lock_irqsave(&l, &flags)` disables local interrupts
  and records the previous state (`RFLAGS.IF`), `spin_unlock_irqrestore`
  restores it — never blind `sti`. With one CPU a spinlock is "disable IRQs +
  (debug) ownership assert"; the API is shaped so SMP can add the atomic.
* While holding a spinlock: no sleeping, no `kmalloc` (may take the heap lock →
  inversion), no scheduler yield, no I/O waits. Preemption is disabled while a
  spinlock is held.
* Lock order (global, documented here and asserted in debug builds when
  introduced): `sched` → `proc` → `fs` → `mm(heap)` → `mm(pmm)` → `log`.
  `log` is last so any path can print.
* **Sleepable context** = syscall handlers and kernel threads. Only they may
  block via `sched_block()`.
* Interrupt-safe logging: `kprintf` takes the log lock with IRQs disabled;
  panic uses a lock-free path (it may interleave output but never deadlock).
* Single-CPU does **not** remove reentrancy: an IRQ can interrupt any code that
  is not holding IRQs off. Data shared with handlers is protected by
  `irqsave` locks.
* SMP (deferred) will require: real atomic locks, IPI-based TLB shootdown,
  per-CPU run queues, and re-examination of every `irqsave`-only assumption.

## 8. Processes, threads, scheduling (summary; see PROCESS_MODEL.md)

* A **process** owns an address space, fd table, cwd, exit status. A **thread**
  owns kernel stack, saved context, scheduling state. Process → 1..N threads.
* First scheduler: round-robin, fixed timeslice (10 ms planned), one run queue,
  an idle thread that `hlt`s, thread block/wake via wait queues. `arch_context_switch(old, new)`
  saves callee-saved registers + `RSP`; address-space switch (`CR3`) occurs in
  `sched` through an arch call when owner processes differ.
* Starvation test: two CPU-bound user programs each increment a shared-visible
  counter via `write`; both counters must advance within a bounded interval.

## 9. System calls (summary; see SYSCALL_ABI.md)

`SYSCALL/SYSRET` with `RAX`=number, args `RDI,RSI,RDX,R10,R8,R9`, return `RAX`
(negative errno). Initial set: `write`, `exit`, `yield`, `getpid`. Entry
switches to the thread kernel stack and `swapgs`; return uses `SYSRET` only for
canonical `RCX` and falls back to `IRETQ` otherwise (mitigates the non-canonical
return privilege-escalation class). Nothing is implemented before M5.

## 10. VFS and resource ownership (summary; see FILESYSTEM.md)

Distinct objects: **file descriptor** (small integer per process) → **open-file
description** (offset, flags, refcount; shared across `dup`/fork) → **vnode**
(identity of a file in a filesystem; refcounted) → filesystem-private data.
First filesystem: read-only ramfs/initrd delivered as a Limine module
(M8). Persistent storage is post-60-day.

Resource lifecycle rules (applies to processes, threads, frames, fds, files):

1. Every acquire has exactly one creator-owner and a documented release path.
2. Multi-step init uses the "goto cleanup in reverse order" pattern; partial
   init leaves no resource registered/published until complete.
3. Shared resources use reference counts; the last `put` destroys. Counts are
   unsigned and underflow asserts in debug builds.
4. After free, owners clear their pointer (`p = NULL`) to make
   use-after-free fail fast; debug builds poison freed heap memory.

## 11. Drivers

Three layers, kept separate:

1. **Generic interface** in `kernel/` (e.g. `struct block_dev`, console ops).
2. **Arch mechanism** in `arch/` (port I/O, MMIO mapping, MSI/IRQ routing).
3. **Device driver** in `kernel/drivers/` (16550 UART, PS/2, virtio/AHCI/etc.).

Emulator-specific configuration lives only in `scripts/` and `tests/qemu/`. A
common interface (e.g. "block device") does not imply a common driver:
QEMU and VirtualBox expose different default devices (AHCI/NVMe/virtio-blk
availability differs; VirtualBox lacks QEMU's `isa-debug-exit`). The M1 early
UART is arch-provided (port I/O) behind `arch_early_console_putc`.

## 12. Error-handling policy (ADR-0016)

| Class | Handling |
|---|---|
| Recoverable API error | Return `int` ≤ 0 style: `0` success, negative `-E*` (ABI errno values). Callers must check; `[[nodiscard]]`-style `WARN_UNUSED` on fallible APIs. |
| Invalid user input | Validate at the boundary, return `-EINVAL`/`-EFAULT`; never assert on it. |
| Resource exhaustion | Return `-ENOMEM`/`-EMFILE`; callers unwind; no allocation-failure panics outside early boot. |
| Programmer invariant violation | `ASSERT()` (debug builds: panic with file:line; release: also panic for critical invariants via `ASSERT_ALWAYS`). |
| CPU exception | See 7.1. |
| Kernel corruption / fatal init failure | `PANIC()`. |

Functions that return pointers return `NULL` on failure; functions that return
status never return positive values for errors.

Debug aids planned: log levels (`error/warn/info/debug`), register dump (M2),
frame pointers kept (`-fno-omit-frame-pointer`) so a stack walker can be added,
unstripped `build/<cfg>/ataos.elf`, QEMU `-s -S` GDB procedure
(see [DEBUGGING](DEBUGGING.md)). Stack unwinding is deferred.

## 13. Security invariants (summary; see SECURITY.md)

1. User code never runs with kernel privilege; kernel pages never carry the U bit.
2. No page is both writable and executable.
3. Every user-supplied pointer/length passes `access_ok` + guarded copy.
4. All integer arithmetic on sizes from untrusted sources is overflow-checked.
5. Every parser (ELF, filesystem, device data) bounds-checks before use.
6. Failed operations release what they acquired.

## 14. Implemented vs planned (single status table)

| Subsystem | Status | Evidence / note |
|---|---|---|
| Limine boot request/response handling | IMPLEMENTED, TESTED | QEMU BIOS+UEFI boot (see report) |
| Early serial console (COM1) | IMPLEMENTED, TESTED | Serial log captured |
| Freestanding string/mem helpers | IMPLEMENTED, TESTED | Host tests |
| Formatted logging (`kprintf`) | IMPLEMENTED, TESTED | Host tests + boot log |
| Panic / assert | IMPLEMENTED, TESTED | Panic-injection QEMU test |
| QEMU test-exit | IMPLEMENTED, TESTED | Observed exit code in runner |
| GDT/TSS (Task 1) | IMPLEMENTED, TESTED | QEMU BIOS/UEFI + VirtualBox + GDB TR/GDTR/IST check |
| Minimal IDT (Task 2) | IMPLEMENTED, TESTED | QEMU BIOS/UEFI + GDB IDTR/IST gate verification |
| Exception dispatch (Task 3) | IMPLEMENTED, TESTED | Verified across #DE, #UD, #GP, #PF, and #DF via IST |
| PMM | PLANNED (M3) | — |
| Kernel heap | PLANNED (M3) | — |
| VMM / page faults | PLANNED (M4) | — |
| Ring 3 + syscalls | PLANNED (M5) | — |
| Timer + scheduler | PLANNED (M6) | — |
| ELF loader | PLANNED (M7) | — |
| VFS / initrd | PLANNED (M8) | — |
| User runtime / shell | PLANNED (M9) | — |
| VirtualBox boot | IMPLEMENTED, TESTED | Verified via scripts/vbox-run.sh (M1 report) |

`TESTED` here means the specific named tests passed; no subsystem is
`VERIFIED` against full acceptance criteria except where the report says so.
