# Kernel Memory Management (`kernel/mm/`)

## Purpose
Architecture-independent memory management subsystems:
- **Physical Memory Manager (PMM):** Frame allocation bitmap, accounting, and reclamation (M3, implemented/tested).
- **Kernel Heap Allocator:** Header-based first-fit `kmalloc`/`kcalloc`/`kfree` over PMM-backed HHDM arenas (M3/M4, implemented/tested).
- **Virtual Memory Manager (VMM):** Master kernel page tables, mapping policy, W^X/NX enforcement, page-fault reporting and guard pages (M4, implemented/tested).
- **Address spaces:** Private user half, shared kernel half, create/destroy/map/unmap/resolve (M4, implemented/tested).

## Milestone Status
- **M3:** PMM bitmap allocator and initial kernel heap — complete.
- **M4:** Page-table abstraction, address-space descriptors, mapping API and kernel-stack guards — complete; user-stack guards and process-owned leaf teardown are M5/M6 work.

## Contracts & Invariants
- `kernel/mm` must never access control registers (`CR3`, etc.) directly; all paging operations go through the `arch_mm_*` hooks in `include/kernel/arch.h`.
- Allocations enforce integer-overflow checks and alignment guarantees.
- Every page-table frame has one owner; `aspace_destroy` frees private tables and the PML4, while M4 leaf data frames remain caller-owned.
- No kernel leaf is user-accessible or both writable and executable; guard pages stay unmapped.
