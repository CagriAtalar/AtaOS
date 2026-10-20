/*
 * paging_encode.c - pure x86_64 page-table entry encoding and decoding.
 *
 * This translation unit deliberately contains no inline assembly, control
 * register access or physical-memory dereferences. It is shared by the kernel
 * (arch/x86_64/mm/paging.c) and by host unit tests, which exercise the same
 * bit logic without a CPU. Keep it free of kernel-only services.
 *
 * Level-aware decoding matters: bit 7 is PS in a PDPTE/PDE but PAT in a PTE,
 * and bit 12 is PAT in 2 MiB/1 GiB leaves but an address bit in a PTE.
 */
#include "arch/paging.h"
#include "kernel/arch.h"
#include "kernel/panic.h"

uint64_t arch_mm_entry_table(uint64_t table_phys, bool user)
{
    uint64_t entry = (table_phys & MM_ADDR_MASK) | MM_PTE_PRESENT | MM_PTE_WRITE;
    if (user)
        entry |= MM_PTE_USER;
    return entry;
}

uint64_t arch_mm_entry_leaf(uint64_t phys, bool write, bool exec, bool user, bool large)
{
    /* W^X is a hard invariant; callers must never request both (vmm policy
     * rejects the combination well before an entry is built). */
    ASSERT((write && exec) == 0);

    uint64_t entry = (phys & MM_ADDR_MASK) | MM_PTE_PRESENT;
    if (write)
        entry |= MM_PTE_WRITE;
    if (user)
        entry |= MM_PTE_USER;
    if (large)
        entry |= MM_PTE_LARGE;
    /*
     * W^X at the encoding boundary: a leaf is executable iff NX is clear.
     * Simultaneous write+exec pages are rejected by vmm policy before an
     * entry is ever built; the encoding layer keeps X and W independent.
     */
    if (!exec)
        entry |= MM_PTE_NX;
    return entry;
}

uint64_t arch_mm_leaf_reprotect(uint64_t entry, bool write, bool exec, bool user)
{
    ASSERT_ALWAYS(arch_mm_entry_present(entry));

    /*
     * Preserve everything except W, U and NX. This deliberately keeps PS,
     * PAT, PWT, PCD, Accessed, Dirty, Global and the AVL bits exactly as the
     * bootloader left them: re-protection must not silently rewrite cache or
     * page-size attributes. The old implementation rebuilt the entry from
     * the physical mask and therefore dropped all of them.
     */
    uint64_t out = entry & ~(MM_PTE_WRITE | MM_PTE_USER | MM_PTE_NX);
    if (write)
        out |= MM_PTE_WRITE;
    if (user)
        out |= MM_PTE_USER;
    if (!exec)
        out |= MM_PTE_NX;
    return out;
}

void arch_mm_entry_flags(uint64_t entry, unsigned level, struct arch_mm_entry_flags *out)
{
    out->present = arch_mm_entry_present(entry);
    out->write = (entry & MM_PTE_WRITE) != 0;
    out->user = (entry & MM_PTE_USER) != 0;
    out->exec = out->present && (entry & MM_PTE_NX) == 0;
    out->large = (level == 3 || level == 2) && (entry & MM_PTE_LARGE) != 0;
}

bool arch_mm_entry_present(uint64_t entry)
{
    return (entry & MM_PTE_PRESENT) != 0;
}

uint64_t arch_mm_entry_phys(uint64_t entry)
{
    return entry & MM_ADDR_MASK;
}

uint64_t arch_mm_entry_leaf_phys(uint64_t entry, unsigned level)
{
    ASSERT_ALWAYS(level >= 1 && level <= 3);
    uint64_t phys = entry & MM_ADDR_MASK;
    if (level == 2 || level == 3)
        phys &= ~(1ULL << 12); /* PAT, not address, in a large leaf */
    return phys;
}

bool arch_mm_entry_is_table(uint64_t entry, unsigned level)
{
    if (!arch_mm_entry_present(entry))
        return false;
    if (level == 1)
        return false; /* a PTE is always a leaf */
    if (level == 4)
        return (entry & MM_PTE_LARGE) == 0; /* PS is reserved in a PML4E */
    return (entry & MM_PTE_LARGE) == 0;
}
