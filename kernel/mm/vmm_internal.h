/*
 * vmm_internal.h - private range validation shared by vmm.c and host tests.
 *
 * This is kernel/mm-private. Public mapping callers use kernel/vmm.h.
 */
#ifndef ATAOS_KERNEL_MM_VMM_INTERNAL_H
#define ATAOS_KERNEL_MM_VMM_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

/*
 * Validate a vmm_map() range without touching page tables: alignment, flag
 * policy, kernel-only restriction, dynmap bounds and overflow. Returns 0 or a
 * negative errno. Pure and host-testable.
 */
int vmm_validate_range(uintptr_t vaddr, uint64_t paddr, size_t npages, unsigned flags);

#endif /* ATAOS_KERNEL_MM_VMM_INTERNAL_H */
