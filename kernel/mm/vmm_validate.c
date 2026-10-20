/*
 * vmm_validate.c - pure policy validation for kernel-half mappings (M4).
 *
 * Kept separate from vmm.c so host tests can exercise overflow, alignment and
 * range rules without a CPU or page tables. No allocation, no arch calls.
 */
#include "vmm_internal.h"

#include "kernel/errno.h"
#include "kernel/mm_types.h"
#include "kernel/vmm.h"

int vmm_check_flags(unsigned flags)
{
    if ((flags & ~(VMM_WRITE | VMM_EXEC | VMM_USER)) != 0)
        return -EINVAL;
    if ((flags & VMM_WRITE) != 0 && (flags & VMM_EXEC) != 0)
        return -EINVAL; /* W^X is never granted */
    return 0;
}

int vmm_validate_range(uintptr_t vaddr, uint64_t paddr, size_t npages, unsigned flags)
{
    if ((vaddr & PAGE_OFFSET_MASK) != 0 || (paddr & PAGE_OFFSET_MASK) != 0)
        return -EINVAL;
    if (vmm_check_flags(flags) != 0)
        return -EINVAL;
    if ((flags & VMM_USER) != 0)
        return -EINVAL; /* master mappings are kernel-only */
    if (npages == 0)
        return 0;
    if (npages > (SIZE_MAX / PAGE_SIZE))
        return -EINVAL;

    uint64_t bytes = (uint64_t)npages * PAGE_SIZE;
    if (vaddr < VMM_DYNMAP_BASE || vaddr >= VMM_DYNMAP_END)
        return -EINVAL;
    uint64_t vaddr_end = (uint64_t)vaddr + bytes;
    if (vaddr_end < (uint64_t)vaddr || vaddr_end > VMM_DYNMAP_END)
        return -EINVAL;
    uint64_t paddr_end = paddr + bytes;
    if (paddr_end < paddr)
        return -EINVAL;
    return 0;
}
