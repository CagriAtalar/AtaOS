# Security and Robustness Model

AtaOS is a hobby kernel, not a certified one. The goal is to design basic
protections from the start and to say honestly what is verified.

**Current state (M1):** there is no Ring 3, no paging policy, no syscalls; most
controls below are `PLANNED`. Only freestanding-build hygiene (no stack
protector dependency, no hosted libc, no red zone) and bounded formatting are
implemented.

## 1. Trust boundaries

```
untrusted:  user programs · user pointers · syscall args · ELF files ·
            filesystem contents · device-provided data · bootloader config strings
trusted:    kernel code · early boot (bootloader/firmware are trusted for M1;
            no Secure Boot/measured boot yet)
```

## 2. Controls

| # | Control | Milestone | Verification (planned) | Status |
|---|---|---|---|---|
| S1 | Ring 0/3 separation (GDT, TSS, `SYSCALL` MSRs) | M2/M5 | Ring 3 program executing `cli`/`hlt`/`mov cr3` is killed | PLANNED |
| S2 | Per-process address spaces; kernel half never user-accessible | M4 | Ring 3 read of a kernel address faults; page-table walker asserts no U bit in kernel half | PLANNED |
| S3 | W^X: no mapping is both writable and executable | M4 | `vmm_map(W|X)` returns `-EINVAL`; boot-time walk finds no WX leaf | PLANNED |
| S4 | NX, `CR0.WP`, SMEP/SMAP when available | M4 | Kernel-mode execution of user page faults (where supported by VM) | PLANNED |
| S5 | User-pointer validation + guarded copies | M5 | Bad pointers, wrap-around lengths, kernel addresses all → `-EFAULT` | PLANNED |
| S6 | Integer-overflow checks on sizes (`kcalloc`, ELF segment math, path lengths) | M3+ | Host tests with boundary values | PLANNED |
| S7 | Bounds validation when parsing ELF / FS / device data | M7/M8 | Fuzz-style malformed fixtures in host tests | PLANNED |
| S8 | Error-path cleanup | all | Frame counters unchanged after failed ops | PLANNED |
| S9 | Invalid syscalls return `-ENOSYS`, never crash | M5 | Test calls number 9999 | PLANNED |
| S10 | Fault in user mode terminates only that process | M5 | A faulting program does not stop a second one | PLANNED |
| S11 | Resource exhaustion returns errors; per-process fd and memory caps | M7/M8 | Test exhausts frames/fds | PLANNED |
| S12 | Interrupt/reentrancy discipline (IRQ-safe locks, no blocking in IRQ) | M6 | Review + stress test | PLANNED |
| S13 | Kernel stack guard pages | M4/M6 | Overflow test faults instead of corrupting memory | PLANNED |
| S14 | Pinned and checksum-verified bootloader download | M1 | `scripts/fetch-limine.sh` rejects wrong checksum | IMPLEMENTED (rejection test: see report) |

## 3. Untrusted-input rules

* ELF: validate magic, class, endianness, type, machine, `e_phoff/e_phnum`
  against file size, each `PT_LOAD` (`p_offset+p_filesz` within the file, no
  overflow, `p_memsz ≥ p_filesz`, vaddr range inside user space, aligned, not
  overlapping, no W+X), entry inside an executable segment.
* Filesystem metadata: every length/offset checked against the containing
  object before dereference.
* Boot data: `bootinfo` capture bounds the memory-map copy and string copies.

## 4. Known limitations (initial implementation)

* The bootloader, firmware, and `limine.conf` are trusted; no Secure Boot or
  measured boot.
* No ASLR/KASLR (disabled for debuggability).
* The M1 boot stack has no guard page.
* No speculative-execution mitigations; no KPTI.
* No entropy source; `limine` entropy feature not yet consumed.
* Test-only QEMU exit port exists only in `TEST=1` builds; not present in
  default builds (verified by symbol/ISO inspection, see report).
* "A check exists" is not "the subsystem is secure": each control above is
  only as strong as its tests and the surrounding invariants.
