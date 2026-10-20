# ADR-0007: Virtual address-space and kernel mapping policy

- **Status:** accepted (design; implemented in M4)
- **Date:** 2026-10-13

## Context and problem

Define how kernel and user mappings coexist, what is shared between address
spaces, and how permissions are enforced.

## Considered alternatives

1. **Shared kernel half (copy upper PML4 entries into each space)** — simple, fast
   syscalls/interrupts, no CR3 switch on kernel entry.
2. **Separate kernel and user page tables (KPTI-style)** — mitigates Meltdown;
   needs trampoline mappings and CR3 switches on every entry; premature.
3. **Build every address space from scratch** — more code and the kernel half
   would need identical updates everywhere.

## Decision

Option 1. A master kernel PML4 is built in M4 from Limine's mappings,
re-protected (NX, `W^X`, RO rodata). Each new process PML4 copies entries
256–511 and zeroes 0–255. Kernel top-level slots that will ever be used (heap,
stacks, HHDM) exist before any process is created. `vmm_map` rejects `W|X`;
user mappings always set U; kernel mappings never do. 4 KiB pages first. Layout in
MEMORY_LAYOUT.md.

## Rationale

Simplest correct design for a single-CPU educational kernel; leaves the door
open to KPTI by changing only the entry/exit paths.

## Trade-offs and consequences

No Meltdown mitigation. Kernel memory is mapped (supervisor-only) while user code
runs; SMEP/SMAP/NX/WP are the guards.

## Testing and verification implications

Self-tests: map/unmap invariants, user access to kernel address faults, WX
reject, address-space destroy returns all frames (PMM counters), page-table
walker asserting no U bit in the kernel half.

## Conditions that could justify revisiting

Enabling KPTI, 5-level paging, or per-CPU mapping needs for SMP.

## Amendment (M4, 2026-10-20): guards, page faults and leaf ownership

* **Guard pages (E2):** a dedicated, page-aligned 4 KiB page sits below the
  16 KiB boot stack and below each 4 KiB IST stack. The boot copy of the guard
  is present, so `vmm_init()` removes it from the master PML4 after copying the
  kernel half and before loading CR3. Pre-flight asserts each guard is absent;
  the audit counts guards separately so every remaining data page is still
  proven RW+NX. Cost: 16 KiB more `.bss`, zero extra frames, four pre-flight
  specs. Risk: a bootloader large leaf covering a guard is a boot panic rather
  than a silent split. User-stack guards and per-thread kernel stacks are M5/M6
  work; IST1 overflow itself remains a triple-fault risk.
* **#PF reporting:** the dispatcher decodes P/W/U/RSVD/I-D/PK/SGX, prints CR2,
  origin/access/cause/fetch and a W^X/NX/not-present hint. Kernel-origin #PF is
  fatal in M4; user-origin termination requires M5.
* **Leaf ownership (G4):** `aspace_destroy()` frees only private page tables and
  the PML4; leaf data frames remain caller-owned until M5 process teardown. The
  M5 follow-up is named in `ROADMAP.md` and `docs/PROCESS_MODEL.md`.
* **Page-table errors:** a present non-table PML4 entry or an intersecting large
  leaf that cannot be split is a panic with the leaf range, level and kernel
  section; it is never silently ignored.
