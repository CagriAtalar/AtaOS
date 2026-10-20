/*
 * vmm.h - virtual memory mapping policy (M4).
 *
 * Builds the master kernel page tables from the bootloader mappings
 * (ADR-0007), enforces W^X, and provides kernel-half map/unmap and
 * resolve. Architecture-independent: all page-table encodings go through
 * the arch hooks in include/kernel/arch.h.
 */
#ifndef ATAOS_KERNEL_VMM_H
#define ATAOS_KERNEL_VMM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct bootinfo;

/*
 * Mapping permission flags for vmm_map() / aspace map calls (M4/M5).
 * Read is always implied. WRITE and EXEC are mutually exclusive (W^X);
 * USER marks Ring 3-traversable pages and is rejected by the kernel-only
 * mapping functions.
 */
#define VMM_WRITE (1U << 0)
#define VMM_EXEC  (1U << 1)
#define VMM_USER  (1U << 2)

/* Highest canonical user address (exclusive); derived from MEMORY_LAYOUT §2. */
#define VMM_USER_TOP 0x0000800000000000ULL

/* Lowest mapped kernel-half address (HHDM default base). */
#define VMM_KERN_HALF_BASE 0xffff800000000000ULL

/* Planned dedicated kernel heap region (pre-populated top-level slot). */
#define VMM_KERN_HEAP_BASE 0xffffc00000000000ULL

/* Planned per-thread kernel stack region (pre-populated top-level slot). */
#define VMM_KERN_STACKS_BASE 0xffffc10000000000ULL

/*
 * Kernel dynamic mapping window (fixmap/dynmap) inside the "kernel image
 * growth, fixmap, modules" reserved region. vmm_map() accepts addresses
 * only here; it never rewires the kernel image or the HHDM.
 */
#define VMM_DYNMAP_BASE 0xffffffffc0000000ULL
#define VMM_DYNMAP_SIZE 0x20000000ULL /* 512 MiB */
#define VMM_DYNMAP_END  (VMM_DYNMAP_BASE + VMM_DYNMAP_SIZE)

/*
 * Pure flag-policy check. Returns 0 when the flags are a legal combination
 * (no WRITE|EXEC, USER allowed only where caller requires it), or -EINVAL.
 * Host-testable (pure function).
 */
int vmm_check_flags(unsigned flags);

/*
 * Build the master kernel page tables:
 *   1. CPU feature checks (CR0.WP, EFER.NXE, SMEP/SMAP when available).
 *   2. Deep-copy the bootloader's kernel half (PML4 entries 256..511) into
 *      kernel-owned frames, re-protecting leaves: text stays RX, everything
 *      else gains NX (HHDM, data, rodata, module regions).
 *   3. Pre-populate the heap/stacks/dynmap top-level slots so mappings into
 *      them never require allocating a new top-level entry after address
 *      spaces exist (ADR-0007).
 *   4. Audit the result: no W|X leaf, no U bit in the kernel half, zeroed
 *      user half.
 *   5. Switch CR3 to the master tables.
 *   6. Run the in-kernel mapping self-test (map/resolve/write/unmap).
 * Panics on any invariant violation (boot-time, deterministic).
 */
void vmm_init(const struct bootinfo *bi);

/*
 * Map `npages` pages into the master kernel address space at `vaddr`
 * (page-aligned) from `paddr` (page-aligned). Only the VMM_DYNMAP window
 * is acceptable. Returns 0, -EINVAL on a malformed range/flags, -EEXIST when
 * any page is already mapped, or -ENOMEM when an intermediate table cannot be
 * allocated. Frames stay owned by the caller: vmm_unmap() only removes
 * translations.
 */
int vmm_map(uintptr_t vaddr, uint64_t paddr, size_t npages, unsigned flags);

/*
 * Remove `npages` page mappings at `vaddr` in the master tables and
 * invalidate them in the CPU. Dynmap table frames are preseeded (ADR-0007)
 * and are deliberately kept after unmaps so mappings after the first
 * address space is created never need new top-level entries. Returns 0, or
 * -EINVAL for a misaligned/out-of-window range, or -ENOENT when any page in
 * the range was not mapped (all mapped pages are still removed).
 */
int vmm_unmap(uintptr_t vaddr, size_t npages);

/*
 * Translate `vaddr` in the master tables. Returns true and sets *paddr_out
 * to the exact physical address (frame or large-page base plus the in-page
 * offset) when mapped; false when no mapping is present.
 */
bool vmm_resolve(uintptr_t vaddr, uint64_t *paddr_out);

/* Physical address of the master PML4 (usually the active CR3 value). */
uint64_t vmm_master_pml4(void);

#endif /* ATAOS_KERNEL_VMM_H */
