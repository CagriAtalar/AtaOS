/*
 * aspace.c - per-process address-space lifecycle (M4).
 *
 * A new space gets a private, zeroed user half and a shared copy of the
 * master kernel PML4 entries. Destroy walks only the private half; every
 * page-table frame allocated there is returned to the PMM. Leaf data frames
 * are deliberately left to their owner (G4; see docs/MEMORY_LAYOUT.md §6).
 */
#include "kernel/aspace.h"

#include "kernel/arch.h"
#include "kernel/compiler.h"
#include "kernel/errno.h"
#include "kernel/heap.h"
#include "kernel/log.h"
#include "kernel/mm_types.h"
#include "kernel/panic.h"
#include "kernel/pmm.h"

#include "aspace_internal.h"
#include "mm_internal.h"

struct aspace {
    uint64_t pml4;
    size_t table_frames; /* PML4 plus private intermediate tables */
};

static uint64_t s_kernel_pml4 = 0;

void aspace_init(uint64_t kernel_pml4)
{
    ASSERT_ALWAYS(kernel_pml4 != 0);
    ASSERT_ALWAYS(s_kernel_pml4 == 0 || s_kernel_pml4 == kernel_pml4);
    s_kernel_pml4 = kernel_pml4;
}

int aspace_create_from(uint64_t kernel_pml4, struct aspace **out)
{
    if (!out)
        return -EINVAL;
    *out = NULL;
    if (kernel_pml4 == 0)
        return -EINVAL;

    struct aspace *as = kcalloc(1, sizeof(*as));
    if (!as)
        return -ENOMEM;

    uint64_t pml4 = mm_alloc_table_try(NULL);
    if (pml4 == 0) {
        kfree(as);
        return -ENOMEM;
    }

    as->pml4 = pml4;
    as->table_frames = 1;

    /* Shared kernel half: same page-table subtrees as the master space. */
    for (unsigned i = MM_PML4_KERNEL_FIRST; i < ARCH_MM4_TABLE_ENTRIES; i++)
        arch_mm_table_write(pml4, i, arch_mm_table_read(kernel_pml4, i));

    /* The user half was zeroed by mm_alloc_table_try(). */
    *out = as;
    return 0;
}

int aspace_create(struct aspace **out)
{
    if (s_kernel_pml4 == 0)
        return -EINVAL;
    return aspace_create_from(s_kernel_pml4, out);
}

void aspace_destroy(struct aspace *as)
{
    if (!as)
        return;

    for (unsigned i = 0; i < MM_PML4_KERNEL_FIRST; i++) {
        uint64_t entry = arch_mm_table_read(as->pml4, i);
        struct arch_mm_entry_flags f;
        arch_mm_entry_flags(entry, MM_LEVEL_PML4, &f);
        if (!f.present)
            continue;
        ASSERT_ALWAYS(arch_mm_entry_is_table(entry, MM_LEVEL_PML4));
        mm_free_subtree(arch_mm_entry_phys(entry), MM_LEVEL_PDPT, &as->table_frames);
    }

    uint64_t pml4 = as->pml4;
    ASSERT_ALWAYS(as->table_frames == 1);
    mm_free_table(pml4, &as->table_frames);
    kfree(as);
}

uint64_t aspace_pml4(const struct aspace *as)
{
    return as ? as->pml4 : 0;
}

size_t aspace_table_frames(const struct aspace *as)
{
    return as ? as->table_frames : 0;
}

static int validate_user_range(uintptr_t vaddr, size_t npages, unsigned flags)
{
    if ((vaddr & PAGE_OFFSET_MASK) != 0)
        return -EINVAL;
    if ((flags & ~(VMM_WRITE | VMM_EXEC | VMM_USER)) != 0)
        return -EINVAL;
    if ((flags & VMM_WRITE) != 0 && (flags & VMM_EXEC) != 0)
        return -EINVAL;
    if ((flags & VMM_USER) == 0)
        return -EINVAL;
    if (npages == 0)
        return 0;
    if (npages > (SIZE_MAX / PAGE_SIZE))
        return -EINVAL;

    uint64_t end = (uint64_t)vaddr + (uint64_t)npages * PAGE_SIZE;
    if (end < (uint64_t)vaddr || (uint64_t)vaddr >= VMM_USER_TOP ||
        end > VMM_USER_TOP)
        return -EINVAL;
    return 0;
}

int aspace_map(struct aspace *as, uintptr_t vaddr, uint64_t paddr, size_t npages,
               unsigned flags)
{
    if (!as)
        return -EINVAL;
    if (npages == 0)
        return 0;
    if ((paddr & PAGE_OFFSET_MASK) != 0)
        return -EINVAL;
    int status = validate_user_range(vaddr, npages, flags);
    if (status != 0)
        return status;

    uint64_t bytes = (uint64_t)npages * PAGE_SIZE;
    if (paddr + bytes < paddr)
        return -EINVAL;

    for (size_t i = 0; i < npages; i++) {
        struct mm_query q;
        mm_query_mapping(as->pml4, vaddr + i * PAGE_SIZE, &q);
        if (q.present)
            return -EEXIST;
    }

    size_t mapped = 0;
    for (size_t i = 0; i < npages; i++) {
        uint64_t page_vaddr = vaddr + i * PAGE_SIZE;
        uint64_t leaf = arch_mm_entry_leaf(paddr + i * PAGE_SIZE,
                                           (flags & VMM_WRITE) != 0,
                                           (flags & VMM_EXEC) != 0, true, false);
        enum mm_set_result result = mm_set_leaf(as->pml4, page_vaddr, leaf, true,
                                                &as->table_frames, NULL);
        if (result == MM_SET_NOMEM) {
            status = -ENOMEM;
            break;
        }
        if (result == MM_SET_EXISTS || result == MM_SET_BLOCKED) {
            status = -EEXIST;
            break;
        }
        mapped++;
        arch_mm_invlpg(page_vaddr);
    }

    if (status != 0) {
        for (size_t i = 0; i < mapped; i++)
            (void)mm_clear_leaf(as->pml4, vaddr + i * PAGE_SIZE, true,
                                &as->table_frames);
        return status;
    }
    return 0;
}

int aspace_unmap(struct aspace *as, uintptr_t vaddr, size_t npages)
{
    if (!as)
        return -EINVAL;
    if (npages == 0)
        return 0;

    int status = validate_user_range(vaddr, npages, VMM_USER);
    if (status != 0)
        return status;

    status = 0;
    for (size_t i = 0; i < npages; i++) {
        if (!mm_clear_leaf(as->pml4, vaddr + i * PAGE_SIZE, true,
                           &as->table_frames))
            status = -ENOENT;
    }
    return status;
}

bool aspace_resolve(const struct aspace *as, uintptr_t vaddr, uint64_t *paddr_out)
{
    if (!as || !paddr_out)
        return false;
    return mm_translate(as->pml4, vaddr, paddr_out);
}

void aspace_self_test(void)
{
    struct pmm_stats before;
    struct pmm_stats after;
    pmm_get_stats(&before);

    struct aspace *as = NULL;
    ASSERT_ALWAYS(aspace_create(&as) == 0);
    ASSERT_ALWAYS(aspace_table_frames(as) == 1);

    uintptr_t frame = pmm_alloc_frame();
    ASSERT_ALWAYS(frame != PADDR_INVALID && frame != 0);
    const uintptr_t user_va = 0x0000000000400000ULL;

    ASSERT_ALWAYS(aspace_map(as, user_va, frame, 1, VMM_WRITE | VMM_USER) == 0);
    ASSERT_ALWAYS(aspace_table_frames(as) == 4);
    uint64_t resolved = 0;
    ASSERT_ALWAYS(aspace_resolve(as, user_va + 0x123, &resolved));
    ASSERT_ALWAYS(resolved == (uint64_t)frame + 0x123);

    struct mm_query q;
    mm_query_mapping(aspace_pml4(as), user_va, &q);
    struct arch_mm_entry_flags f;
    arch_mm_entry_flags(q.entry, q.level, &f);
    ASSERT_ALWAYS(q.present && q.leaf && f.user && f.write && !f.exec);

    ASSERT_ALWAYS(aspace_map(as, user_va, frame, 1, VMM_WRITE | VMM_USER) == -EEXIST);
    ASSERT_ALWAYS(aspace_map(as, user_va + PAGE_SIZE, frame, 1, VMM_WRITE | VMM_EXEC | VMM_USER) ==
                  -EINVAL);
    ASSERT_ALWAYS(aspace_map(as, user_va + PAGE_SIZE, frame, 1, VMM_WRITE) == -EINVAL);
    ASSERT_ALWAYS(aspace_map(as, VMM_KERN_HALF_BASE, frame, 1, VMM_WRITE | VMM_USER) == -EINVAL);

    ASSERT_ALWAYS(aspace_unmap(as, user_va, 1) == 0);
    ASSERT_ALWAYS(!aspace_resolve(as, user_va, &resolved));
    ASSERT_ALWAYS(aspace_unmap(as, user_va, 1) == -ENOENT);
    ASSERT_ALWAYS(aspace_table_frames(as) == 1);

    /* The shared kernel half must resolve through the new space too. */
    ASSERT_ALWAYS(aspace_resolve(as, (uintptr_t)&aspace_self_test, &resolved));

    pmm_free_frame(frame);
    aspace_destroy(as);

    pmm_get_stats(&after);
    ASSERT_ALWAYS(after.used_frames == before.used_frames);
    ASSERT_ALWAYS(after.free_frames == before.free_frames);

#ifndef ATAOS_HOST_TEST
    log_info("aspace: self-test passed (create/map/resolve/destroy; PMM used %lu -> %lu)",
             (unsigned long)before.used_frames, (unsigned long)after.used_frames);
#endif
}
