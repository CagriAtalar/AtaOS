# Kernel Memory Management (`kernel/mm/`)

## Purpose
Architecture-independent memory management subsystems:
- **Physical Memory Manager (PMM):** Frame allocation bitmap, accounting, and reclamation (Milestone M3).
- **Kernel Heap Allocator:** Freestanding dynamic allocation (`kmalloc`, `kfree`) using slab/freelist (Milestone M3).
- **Virtual Memory Manager (VMM):** Architecture-independent address space policy, mapping abstractions, and guard pages (Milestone M4).

## Planned Milestones
- **M3 (Physical Memory & Kernel Allocation):** PMM bitmap allocator and initial kernel heap.
- **M4 (Virtual Memory):** Page-table abstraction, address-space descriptors, and mapping API.

## Contracts & Invariants
- Must never interact directly with x86_64 page-table registers (CR3); all architecture paging operations must go through `arch_vmm_*` interfaces.
- Allocations must enforce integer overflow checks and alignment guarantees.
