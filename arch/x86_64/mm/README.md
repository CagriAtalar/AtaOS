# x86_64 Hardware Paging (`arch/x86_64/mm/`)

## Purpose
Hardware-level 4-level paging implementation for x86_64:
- PML4, PDPT, PD, and PT entry manipulation and bitflags (`PAGE_PRESENT`, `PAGE_WRITABLE`, `PAGE_USER`, `PAGE_NX`).
- Page table walk and recursive / HHDM-based page table manipulation.
- TLB invalidation (`invlpg`) and CR3 control.

## Planned Milestones
- **M4 (Virtual Memory):** x86_64 page-table management, kernel mapping cloning, and user address-space construction.

## Contracts & Invariants
- Kernel memory mappings (`0xffff800000000000`+) must never have the `PAGE_USER` bit set.
- All page table allocations must come from the physical memory manager and be mapped via the Higher-Half Direct Map (HHDM).
