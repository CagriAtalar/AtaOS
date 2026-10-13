# AtaOS Requirements

Status vocabulary (see [AGENTS.md](../AGENTS.md)): `PLANNED`, `IMPLEMENTED`,
`TESTED`, `VERIFIED`, `BLOCKED`. A requirement is `VERIFIED` only when **every**
acceptance criterion listed for it has passed. Priorities: **P0** = needed for
the first 60 days, **P1** = needed for a usable system, **P2** = deferred.

Milestone names (M1–M12) are defined in [ROADMAP.md](../ROADMAP.md).

> Status as of the initial bootstrap session: only M1 work exists. Every
> requirement not mentioning M1 is `PLANNED`. See
> [reports/initial-bootstrap.md](reports/initial-bootstrap.md) for evidence.

## 1. Functional requirements

| ID | Description | Pri | Deps | Milestone | Acceptance criteria | Verification | Status |
|---|---|---|---|---|---|---|---|
| FR-001 | Boot via Limine on x86_64 long mode into the higher-half kernel, with a required-response check | P0 | AR-001 | M1 | Kernel ELF is loaded; banner appears on COM1; missing base revision/HHDM/memmap response causes a `PANIC` with a clear message | QEMU smoke test (BIOS, UEFI); manual VirtualBox | TESTED (QEMU BIOS/UEFI, see report); VirtualBox NOT RUN |
| FR-002 | Early serial console and formatted kernel logging | P0 | FR-001 | M1 | `kprintf`/`log_*` output reaches COM1; supported specifiers documented; unsupported ones do not crash | Host tests (formatter), QEMU log | TESTED |
| FR-003 | Boot information summary (bootloader, firmware type, HHDM, memory map, framebuffer, RSDP) | P0 | FR-001 | M1 | Each item appears in serial log in a documented format | QEMU smoke test | TESTED |
| FR-004 | Panic and assertion support usable before any subsystem exists | P0 | FR-002 | M1 | `PANIC()` prints file:line, disables interrupts, halts; never allocates | QEMU panic-injection test (`TEST_PANIC=1`) | TESTED |
| FR-005 | CPU exceptions handled via GDT/TSS/IDT with register dump on fatal faults | P0 | FR-004 | M2 | Deliberate #DE, #UD, #GP, #PF, #DF are reproduced and reported with vector, error code, RIP, CR2 | QEMU fault-injection tests | PLANNED |
| FR-006 | Hardware interrupts via interrupt controller, with acknowledgement and dispatch table | P0 | FR-005 | M6 (controller bring-up begins M2/M6) | Timer IRQ fires at configured rate; spurious IRQs counted not crashed | QEMU test, VirtualBox manual | PLANNED |
| FR-007 | Physical memory manager built from the Limine memory map | P0 | FR-003 | M3 | Reserved regions never returned; alloc/free round-trip leaves counters unchanged; alignment and overflow tests pass | Host tests on bitmap logic; QEMU self-test | PLANNED |
| FR-008 | Virtual memory manager: 4-level paging, map/unmap, permission bits, address-space create/destroy | P0 | FR-007, FR-005 | M4 | Mapped page readable/writable per flags; unmap faults; destroy frees all owned frames; kernel pages never user-accessible | QEMU self-tests | PLANNED |
| FR-009 | Kernel dynamic allocation (`kmalloc`/`kfree`/`kcalloc`) | P0 | FR-007, FR-008 | M3 (initial), M4 (heap region) | Alignment ≥ 16; overflow in `kcalloc` rejected; double-free detected in debug; stress test leak-free | Host tests on allocator core; QEMU self-test | PLANNED |
| FR-010 | User/kernel isolation using Ring 3 and per-process address spaces | P0 | FR-008 | M5 | User code cannot execute privileged instructions or touch kernel memory; faults kill only the process | QEMU Ring-3 negative tests | PLANNED |
| FR-011 | System calls (`write`, `exit`, `yield`, `getpid` initially) | P0 | FR-010 | M5 | Ring 3 program invokes each documented syscall and gets documented results; invalid pointers return `-EFAULT` | QEMU user test program | PLANNED |
| FR-012 | Processes and threads with a documented lifecycle | P0 | FR-008, FR-010 | M6 | Create/exit/reap leaves frame count unchanged | QEMU self-test | PLANNED |
| FR-013 | Preemptive single-CPU round-robin scheduling | P0 | FR-006, FR-012 | M6 | Two CPU-bound threads both make progress; idle thread runs when none runnable | QEMU test with progress counters | PLANNED |
| FR-014 | ELF64 static executable loading (limited subset) | P0 | FR-008, FR-012 | M7 | Valid program runs; each malformed-header fixture rejected; failed load leaks no frames | Host tests for validator; QEMU tests | PLANNED |
| FR-015 | VFS with per-process file-descriptor tables and cwd | P1 | FR-012 | M8 | `open/read/write/close` on ramfs; per-process cwd independent; bad fd → `-EBADF` | QEMU user test | PLANNED |
| FR-016 | Initial ramfs/initrd filesystem (Limine module) | P1 | FR-015 | M8 | Files in initrd readable by user program | QEMU test | PLANNED |
| FR-017 | User-space runtime, minimal libc, `init`, shell, basic utilities | P1 | FR-011, FR-014, FR-015 | M9 | Shell starts from `init`, runs two utilities, reports exit status | QEMU scripted-input test | PLANNED |
| FR-018 | Keyboard input and text/framebuffer display | P1 | FR-006 | after M9 | Scancodes decoded; text visible on framebuffer | Manual + QEMU screendump | PLANNED (deferred) |
| FR-019 | Persistent filesystem and block driver | P2 | FR-015 | post-60-day | Files survive reboot | QEMU disk image test | PLANNED (deferred) |
| FR-020 | Networking | P2 | FR-019 | post-60-day | — | — | PLANNED (deferred) |
| FR-021 | GUI and large applications (e.g. Doom) | P2 | FR-018, FR-019 | post-60-day | — | — | PLANNED (deferred) |

## 2. Architectural requirements

| ID | Description | Pri | Deps | Milestone | Acceptance criteria | Verification | Status |
|---|---|---|---|---|---|---|---|
| AR-001 | Monolithic kernel with module boundaries and a documented dependency direction | P0 | — | M1 | `docs/ARCHITECTURE.md` dependency table matches directory layout; `scripts/check-deps.sh` reports no forbidden includes | Script + review | IMPLEMENTED (script covers `kernel/`, `include/`, `arch/` include rules) |
| AR-002 | Architecture-specific code isolated in `arch/x86_64/` behind `include/kernel/arch.h` | P0 | AR-001 | M1 | No `asm`, port I/O or MSR access outside `arch/` | `scripts/check-deps.sh` | IMPLEMENTED |
| AR-003 | Kernel never links hosted libc; freestanding utilities live in `kernel/lib/` | P0 | — | M1 | Link of `ataos.elf` succeeds with `-nostdlib`; no undefined symbols | Build | VERIFIED (build only; see report) |
| AR-004 | Higher-half link address compatible with `-mcmodel=kernel` and Limine | P0 | — | M1 | ELF `PT_LOAD` vaddrs ≥ `0xffffffff80000000`; kernel boots | `readelf`, QEMU boot | TESTED |
| AR-005 | User programs cannot include kernel-private headers | P0 | AR-001 | M5 | `user/` builds with `-I include/abi` only | Build rule + script | PLANNED |
| AR-006 | Syscall ABI isolated in `include/abi/` and documented in `docs/SYSCALL_ABI.md` | P0 | — | M5 | ABI header and document agree on numbers and error codes | Review + test | PLANNED |
| AR-007 | Bootloader-owned data is never held by pointer after boot-info retirement | P0 | FR-003 | M1 (copy-out API), M3 (retirement) | Boot-info accessors copy by value; PMM reclaims bootloader-reclaimable only after retirement | Review + M3 test | IMPLEMENTED (API); retirement PLANNED |

## 3. Quality requirements

| ID | Description | Pri | Deps | Milestone | Acceptance criteria | Verification | Status |
|---|---|---|---|---|---|---|---|
| QR-001 | Build reproducibility: pinned toolchain assumptions, pinned Limine, no timestamps in binaries | P0 | — | M1 | Two clean builds of the same tree and commit yield byte-identical `ataos.elf` | `make repro-check` | TESTED (see report) |
| QR-002 | Memory safety and error handling per the error policy (ADR-0016) | P0 | — | all | Every acquire has a documented release; untrusted input validated | Review checklist + tests | PLANNED (policy documented) |
| QR-003 | Testability: pure logic is host-testable | P0 | — | M1 | `make test-host` passes with ASan+UBSan | Host tests | TESTED |
| QR-004 | Debuggability: unstripped ELF, frame pointers, serial log, GDB stub procedure | P0 | — | M1 | `make run-debug` + documented GDB session reaches `kmain` | Manual GDB session | NOT RUN (see report) |
| QR-005 | Documentation accuracy; commands verified against the Makefile | P0 | — | all | Doc review checklist passes | Review | IMPLEMENTED |
| QR-006 | QEMU/VirtualBox portability: same kernel and ISO | P0 | FR-001 | M1, M10 | Boot on both with documented VM settings | QEMU automated; VirtualBox manual | QEMU TESTED; VirtualBox NOT RUN |
| QR-007 | Maintainability: files < ~500 lines where practical; style enforced by `.clang-format` | P1 | — | all | `make format-check` passes | clang-format | NOT RUN (clang-format not installed) |
| QR-008 | Security boundaries per `docs/SECURITY.md` | P0 | FR-010 | M4–M5 | Each listed control has a test or an explicit "unverified" note | Tests + review | PLANNED |
| QR-009 | Regression testing: every bug fix adds a test when feasible | P0 | — | all | Bug-fix commits reference a test | Review | PLANNED (process) |
| QR-010 | Failure diagnosis: failing tests print log tail and exit code meaning | P0 | — | M1 | Runner prints tail of serial log on failure | Run runner against a deliberately failing kernel (`TEST_PANIC=1`) | TESTED |

## 4. Compatibility requirements

| ID | Description | Pri | Milestone | Acceptance | Verification | Status |
|---|---|---|---|---|---|---|
| CR-001 | QEMU q35, BIOS firmware | P0 | M1 | Boot marker in serial log | `make test-qemu` | TESTED |
| CR-002 | QEMU q35, UEFI (OVMF) | P0 | M1 | Boot marker in serial log | `make test-qemu` | TESTED |
| CR-003 | VirtualBox BIOS | P0 | M10 (M1 manual) | Boot marker in serial log file | Manual per [VIRTUALBOX.md](VIRTUALBOX.md) | NOT RUN |
| CR-004 | VirtualBox UEFI | P0 | M10 (M1 manual) | Boot marker | Manual | NOT RUN |
| CR-005 | No dependency on KVM or host CPU-specific features | P0 | all | Tests pass under TCG | QEMU run without `-enable-kvm` | TESTED |

## 5. Deferred goals and non-goals (initial milestones)

Explicitly deferred; agents must **not** implement them unprompted:
SMP/multi-core scheduling, USB, advanced power management (beyond what is
required to halt safely), dynamic kernel modules, complete POSIX, full network
protocols, desktop/window manager, sound, a Doom port, additional CPU
architectures.

SMP-adjacent *design hooks* (per-CPU data struct, lock API that can be upgraded)
are allowed because they are documented in ARCHITECTURE.md; SMP behaviour is not.
