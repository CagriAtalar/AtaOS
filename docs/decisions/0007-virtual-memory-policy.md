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
