# AtaOS 60-Day Development Roadmap

This document outlines the 12 incremental milestones across approximately 60 days of supervised development. The calendar timeline is an estimate; verifiable milestone acceptance criteria strictly govern milestone completion.

---

## Roadmap Overview (60-Day Schedule)

| Milestone | Window | Focus Area | Status |
|---|---|---|---|
| **M1** | Days 1–7 | Repository & Boot Foundation | **IMPLEMENTED / TESTED** |
| **M2** | Days 8–12 | CPU Fundamentals (GDT, TSS, IDT, Exceptions) | PLANNED |
| **M3** | Days 13–18 | Physical Memory & Kernel Allocation (PMM, Heap) | PLANNED |
| **M4** | Days 19–24 | Virtual Memory Management (Paging, Address Spaces) | PLANNED |
| **M5** | Days 25–30 | Ring 3 & System Calls (Syscall ABI, User Isolation) | PLANNED |
| **M6** | Days 31–36 | Timer & Preemptive Scheduler (APIC, Round-Robin) | PLANNED |
| **M7** | Days 37–41 | ELF64 Loading & Process Lifecycle | PLANNED |
| **M8** | Days 42–46 | Virtual Filesystem & Initial Ramfs | PLANNED |
| **M9** | Days 47–51 | User-Space Environment (init, Shell, Minimal Libc) | PLANNED |
| **M10** | Days 52–55 | Stability & Virtual Machine Compatibility (QEMU/VBox) | PLANNED |
| **M11** | Days 56–58 | Integration, Memory Audits & Hardening | PLANNED |
| **M12** | Days 59–60 | Release Checkpoint & Reproducibility Audit | PLANNED |

---

## Detailed Milestone Specifications

### M1 — Repository and Boot Foundation
- **Expected Window:** Days 1–7
- **Goal:** Establish build and test infrastructure, architectural contracts, Limine boot protocol integration, early serial logging, and panic handling.
- **Scope:**
  - Complete architecture documentation (`docs/`) and initial ADRs (ADR-0001 through ADR-0016).
  - Freestanding x86_64 toolchain configuration (Clang + LLD).
  - Limine v12.9.3 boot integration (hybrid BIOS/UEFI ISO).
  - Kernel higher-half entry (`0xffffffff80000000`).
  - Early COM1 serial console driver and formatted output (`kprintf`, `klog`).
  - Panic and assertion subsystem (`PANIC`, `ASSERT`).
  - Host unit tests with AddressSanitizer/UBSan for freestanding utilities.
  - Automated headless QEMU smoke test runner (`tests/qemu/run.py`).
  - VirtualBox test procedure documentation (`docs/COMPATIBILITY.md`).
- **Explicit Exclusions:** GDT/TSS, IDT, interrupts, dynamic heap, paging management, user mode, scheduler, filesystem.
- **Acceptance Criteria:**
  - Kernel builds with `-nostdlib` and zero warnings under `-Werror`.
  - ISO boots in QEMU under BIOS and UEFI firmware modes.
  - Serial log prints boot banner, bootloader info, memory map summary, and `[ataos] boot complete`.
  - Host tests pass clean under ASan and UBSan.
  - Test-only QEMU exit device halts with status 33 on success and 35 on panic.
- **Required Tests:** `make test-host`, `make test-scripts`, `make test-qemu`.
- **Current Status:** **VERIFIED** (QEMU BIOS, QEMU UEFI, and VirtualBox BIOS verified; host unit tests and reproducible builds passed).

---

### M2 — CPU Fundamentals
- **Expected Window:** Days 8–12
- **Goal:** Establish kernel control of CPU descriptor tables, exception handling, and fault reporting.
- **Scope:**
  - GDT and TSS initialization with dedicated IST stacks.
  - IDT initialization (vectors 0–31).
  - Common assembly interrupt entry/exit stubs pushing `struct trap_frame`.
  - C exception dispatcher with detailed register dumps on unhandled faults.
  - CPU control register sanity checks (`CR0.WP`, `EFER.NXE`).
- **Explicit Exclusions:** Hardware IRQs, APIC timer, user-mode syscall stubs.
- **Acceptance Criteria:**
  - Deliberate fault injection (#DE, #UD, #GP, #PF) triggers structured panic with register dump and halts cleanly.
  - Double fault (#DF) handled safely via IST stack.
- **Current Status:** PLANNED (Task 1 GDT/TSS and Task 2 Minimal Safe IDT implemented and tested; Task 3 exception dispatcher pending).

---

### M3 — Physical Memory & Kernel Allocation
- **Expected Window:** Days 13–18
- **Goal:** Provide reliable physical frame allocation and a minimal kernel heap.
- **Scope:**
  - Parse Limine memory map into bitmap-tracked physical page manager (PMM).
  - Frame allocation and free API (`pmm_alloc_frame`, `pmm_free_frame`).
  - Memory accounting and leak tracking counters.
  - Initial kernel heap allocator (`kmalloc`, `kfree`, `kcalloc`) over HHDM.
- **Explicit Exclusions:** Page table modifications, virtual memory manager, user allocations.
- **Acceptance Criteria:**
  - Reserved and firmware regions are never allocated.
  - Frame allocation stress test executes without leaking frames.
  - Heap handles split/coalesce and rejects integer overflows in `kcalloc`.
- **Current Status:** PLANNED.

---

### M4 — Virtual Memory Management
- **Expected Window:** Days 19–24
- **Goal:** Manage 4-level x86_64 paging, enforce page permissions, and support address space lifecycle.
- **Scope:**
  - Master kernel PML4 construction with NX and write-protection flags.
  - Virtual memory mapping API (`vmm_map`, `vmm_unmap`).
  - W^X enforcement (rejection of simultaneous write and execute permissions).
  - Page fault handler (#PF) distinguishing supervisor faults from demand faults.
  - Per-process address space creation and destruction (`aspace_create`, `aspace_destroy`).
- **Explicit Exclusions:** 5-level paging, page swapping to disk.
- **Acceptance Criteria:**
  - Kernel memory inaccessible to user mode (U bit cleared).
  - Access to unmapped page triggers expected fault.
  - Address space destruction frees all owned table frames and returns counters to baseline.
- **Current Status:** PLANNED.

---

### M5 — Ring 3 and System Calls
- **Expected Window:** Days 25–30
- **Goal:** Enter unprivileged Ring 3 execution and handle initial system calls safely.
- **Scope:**
  - User privilege transition (`IRETQ` or `SYSRET`).
  - `SYSCALL`/`SYSRET` MSR configuration (`STAR`, `LSTAR`, `FMASK`).
  - Kernel stack switching via TSS.RSP0 and `swapgs`.
  - User pointer validation helpers (`copy_from_user`, `copy_to_user`).
  - Initial system call set: `write`, `exit`, `yield`, `getpid`.
- **Explicit Exclusions:** Multi-threading, complex file system calls.
- **Acceptance Criteria:**
  - Ring 3 code cannot execute privileged instructions (`cli`, `hlt`, `mov cr3`).
  - Invalid user pointers passed to `write` return `-EFAULT` without crashing the kernel.
  - Test user program completes execution and invokes `exit(0)`.
- **Current Status:** PLANNED.

---

### M6 — Timer & Preemptive Scheduling
- **Expected Window:** Days 31–36
- **Goal:** Enable hardware timer interrupts and implement a preemptive single-CPU round-robin scheduler.
- **Scope:**
  - Local APIC timer initialization and calibration against PIT.
  - Thread control block (`struct thread`) and context switch routines (`arch_context_switch`).
  - Preemptive round-robin run queue with 10ms quantum.
  - Idle thread executing `hlt` loop.
  - Blocking and wake-up mechanism via wait queues.
- **Explicit Exclusions:** Multi-core (SMP), real-time priority classes.
- **Acceptance Criteria:**
  - Two CPU-bound threads execute concurrently and both make observable progress.
  - Runaway loop cannot permanently starve another runnable thread.
  - Terminated threads have their kernel stacks and structures cleanly reaped.
- **Current Status:** PLANNED.

---

### M7 — ELF64 Loading & Process Lifecycle
- **Expected Window:** Days 37–41
- **Goal:** Load static ELF64 binaries from memory and manage process lifecycles.
- **Scope:**
  - ELF64 header and program header validator (bounds, alignments, permissions).
  - `spawn` system call creating address space, loading segments, and creating initial thread.
  - User stack initialization with `argc` and `argv`.
  - Process termination and exit code collection (`wait`).
- **Explicit Exclusions:** Dynamic linking (`.so`), shared libraries, `fork` with COW.
- **Acceptance Criteria:**
  - Valid static ELF64 binary runs and produces correct output.
  - Corrupted or malicious ELF fixtures (overlapping segments, W^X violations) are rejected cleanly.
  - Partial load failures leave no allocated frames leaked.
- **Current Status:** PLANNED.

---

### M8 — Virtual Filesystem & Initial Ramfs
- **Expected Window:** Days 42–46
- **Goal:** Provide unified VFS abstractions and an in-memory archive filesystem.
- **Scope:**
  - VFS interface (`vnode`, open file descriptions, `struct vfs_ops`).
  - Per-process file descriptor table (mapping integers to file objects).
  - Per-process current working directory (`cwd`).
  - Ramfs/initrd driver parsing archives bundled via Limine modules.
  - Syscalls: `open`, `read`, `write`, `close`, `stat`.
- **Explicit Exclusions:** Block storage devices, disk caching layer, disk writing.
- **Acceptance Criteria:**
  - User programs open and read files from ramfs via standard syscalls.
  - Operations on invalid file descriptors return `-EBADF`.
  - Descriptor exhaustion returns `-EMFILE`.
- **Current Status:** PLANNED.

---

### M9 — User-Space Environment
- **Expected Window:** Days 47–51
- **Goal:** Provide minimal user-space runtime, standard library, and interactive shell.
- **Scope:**
  - Minimal freestanding C library for user space (`user/libc/`).
  - Initial user process (`init`) launched at boot.
  - Command-line shell supporting built-in commands (`help`, `echo`, `cat`, `ls`, `exit`).
  - Keyboard input driver (PS/2 keyboard via interrupt).
- **Explicit Exclusions:** Advanced POSIX APIs, job control, signals.
- **Acceptance Criteria:**
  - System boots into `init`, which spawns the interactive shell.
  - Shell reads commands, executes child binaries, and reports exit status.
- **Current Status:** PLANNED.

---

### M10 — Stability & Emulator Compatibility
- **Expected Window:** Days 52–55
- **Goal:** Validate consistent behavior across QEMU and Oracle VirtualBox with regression suites.
- **Scope:**
  - Full regression test execution across BIOS and UEFI boot modes.
  - VirtualBox test verification using documented procedures.
  - Memory leak stress testing under heavy process spawning.
  - Identification and documentation of VM differences in `docs/COMPATIBILITY.md`.
- **Acceptance Criteria:**
  - Test suites pass deterministically across both BIOS and UEFI in QEMU.
  - Manual VirtualBox checklist executed and verified on at least one physical host.
- **Current Status:** PLANNED.

---

### M11 — Integration & Hardening
- **Expected Window:** Days 56–58
- **Goal:** Perform comprehensive code audit, dependency checks, and robustness verification.
- **Scope:**
  - Audit all subsystem interfaces for memory ownership and failure cleanup.
  - Verify dependency isolation via `scripts/check-deps.sh`.
  - Static analysis with `clang-tidy` and `cppcheck` where available.
  - Documentation accuracy review across all documents in `docs/`.
- **Acceptance Criteria:**
  - No known memory leaks, use-after-free, or unchecked user pointers remain.
  - Clean linting reports and zero undocumented dependencies.
- **Current Status:** PLANNED.

---

### M12 — Release Checkpoint
- **Expected Window:** Days 59–60
- **Goal:** Finalize v0.1 release, verify build reproducibility, and document post-60-day backlog.
- **Scope:**
  - Bit-for-bit reproducible build check (`make repro-check`).
  - Final milestone status report and retrospective.
  - Post-60-day backlog grooming (SMP, networking, persistent storage, GUI).
- **Acceptance Criteria:**
  - Two independent clean builds produce byte-identical `ataos.elf` binaries.
  - Release archive and complete documentation package ready.
- **Current Status:** PLANNED.

---

## Deferred Post-60-Day Backlog
Features deferred until core single-CPU stability is complete:
- Symmetric Multiprocessing (SMP / multi-core scheduling).
- USB controller stack (xHCI).
- Block storage device drivers (AHCI SATA / NVMe).
- Persistent filesystems (ext2 or FAT32 write support).
- TCP/IP network stack and Ethernet drivers.
- Graphical window compositor and framebuffer font rendering.
- Doom game port.
