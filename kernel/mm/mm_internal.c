/*
 * mm_internal.c - shared page-table tree walkers for kernel/mm (private).
 *
 * Implements the 4-level walk (indices via ARCH_MM4_* constants from
 * include/kernel/arch.h) and the table-frame bookkeeping shared by vmm.c
 * and aspace.c. Entry encoding/decoding stays behind arch hooks.
 */
#include "mm_internal.h"
#include "kernel/arch.h"
#include "kernel/panic.h"
#include "kernel/pmm.h"

uint64_t mm_canonical_base(unsigned pml4_index)
{
    uint64_t base = (uint64_t)(pml4_index & (ARCH_MM4_TABLE_ENTRIES - 1))
                    << ARCH_MM4_SHIFT_L4;

    /*
     * Sign-extend bit 47 into bits 63..48. The old expression stopped at
     * 48 bits, so PML4 indices 256..511 produced a non-canonical low address
     * instead of the kernel half (D17).
     */
    if ((base & (1ULL << 47)) != 0)
        base |= 0xffff000000000000ULL;
    return base;
}

uint64_t mm_alloc_table_try(size_t *counter)
{
    uintptr_t paddr = pmm_alloc_frame();
    if (paddr == PADDR_INVALID)
        return 0;
    arch_mm_table_zero(paddr);
    if (counter)
        (*counter)++;
    return paddr;
}

uint64_t mm_alloc_table(size_t *counter)
{
    uint64_t frame = mm_alloc_table_try(counter);
    if (frame == 0)
        PANIC("mm: exhausted physical memory allocating a page-table frame");
    return frame;
}

void mm_free_table(uint64_t frame, size_t *counter)
{
    ASSERT_ALWAYS(frame != 0);
    if (counter) {
        ASSERT_ALWAYS(*counter > 0);
        (*counter)--;
    }
    pmm_free_frame(frame);
}

static unsigned shift_for_level(unsigned level)
{
    switch (level) {
    case MM_LEVEL_PML4:
        return (unsigned)ARCH_MM4_SHIFT_L4;
    case MM_LEVEL_PDPT:
        return (unsigned)ARCH_MM4_SHIFT_L3;
    case MM_LEVEL_PD:
        return (unsigned)ARCH_MM4_SHIFT_L2;
    case MM_LEVEL_PT:
        return (unsigned)ARCH_MM4_SHIFT_L1;
    default:
        PANIC("mm: invalid table level %u", level);
    }
}

static unsigned index_of(uint64_t vaddr, unsigned level)
{
    return (unsigned)((vaddr >> shift_for_level(level)) & (ARCH_MM4_TABLE_ENTRIES - 1));
}

enum mm_set_result mm_set_leaf(uint64_t root, uint64_t vaddr, uint64_t leaf_entry, bool user,
                               size_t *counter, uint64_t *out_pt)
{
    if (out_pt)
        *out_pt = 0;

    struct arch_mm_entry_flags f;
    enum mm_set_result result = MM_SET_OK;
    uint64_t pdpt_table = 0;
    uint64_t pd_table = 0;
    uint64_t pt_table = 0;
    bool new_pdpt = false;
    bool new_pd = false;
    bool new_pt = false;

    /* PML4 level: the root table itself is caller-owned and already exists. */
    unsigned l4 = index_of(vaddr, MM_LEVEL_PML4);
    uint64_t pml4e = arch_mm_table_read(root, l4);
    arch_mm_entry_flags(pml4e, MM_LEVEL_PML4, &f);
    if (f.present) {
        if (!arch_mm_entry_is_table(pml4e, MM_LEVEL_PML4))
            return MM_SET_BLOCKED;
        pdpt_table = arch_mm_entry_phys(pml4e);
    } else {
        pdpt_table = mm_alloc_table_try(counter);
        if (pdpt_table == 0)
            return MM_SET_NOMEM;
        new_pdpt = true;
        arch_mm_table_write(root, l4, arch_mm_entry_table(pdpt_table, user));
    }

    unsigned l3 = index_of(vaddr, MM_LEVEL_PDPT);
    uint64_t pml4e3 = arch_mm_table_read(pdpt_table, l3);
    arch_mm_entry_flags(pml4e3, MM_LEVEL_PDPT, &f);
    if (f.present) {
        if (!arch_mm_entry_is_table(pml4e3, MM_LEVEL_PDPT)) {
            result = MM_SET_BLOCKED;
            goto unwind;
        }
        pd_table = arch_mm_entry_phys(pml4e3);
    } else {
        pd_table = mm_alloc_table_try(counter);
        if (pd_table == 0) {
            result = MM_SET_NOMEM;
            goto unwind;
        }
        new_pd = true;
        arch_mm_table_write(pdpt_table, l3, arch_mm_entry_table(pd_table, user));
    }

    unsigned l2 = index_of(vaddr, MM_LEVEL_PD);
    uint64_t pde2 = arch_mm_table_read(pd_table, l2);
    arch_mm_entry_flags(pde2, MM_LEVEL_PD, &f);
    if (f.present) {
        if (!arch_mm_entry_is_table(pde2, MM_LEVEL_PD)) {
            result = MM_SET_BLOCKED;
            goto unwind;
        }
        pt_table = arch_mm_entry_phys(pde2);
    } else {
        pt_table = mm_alloc_table_try(counter);
        if (pt_table == 0) {
            result = MM_SET_NOMEM;
            goto unwind;
        }
        new_pt = true;
        arch_mm_table_write(pd_table, l2, arch_mm_entry_table(pt_table, user));
    }

    unsigned l1 = index_of(vaddr, MM_LEVEL_PT);
    uint64_t pte = arch_mm_table_read(pt_table, l1);
    arch_mm_entry_flags(pte, MM_LEVEL_PT, &f);
    if (f.present)
        return MM_SET_EXISTS;
    if (!arch_mm_entry_present(leaf_entry)) {
        result = MM_SET_BLOCKED;
        goto unwind;
    }

    arch_mm_table_write(pt_table, l1, leaf_entry);
    if (out_pt)
        *out_pt = pt_table;
    return MM_SET_OK;

unwind:
    if (new_pt) {
        arch_mm_table_write(pd_table, l2, 0);
        mm_free_table(pt_table, counter);
    }
    if (new_pd) {
        arch_mm_table_write(pdpt_table, l3, 0);
        mm_free_table(pd_table, counter);
    }
    if (new_pdpt) {
        arch_mm_table_write(root, l4, 0);
        mm_free_table(pdpt_table, counter);
    }
    return result;
}

bool mm_translate(uint64_t root, uint64_t vaddr, uint64_t *paddr_out)
{
    struct arch_mm_entry_flags f;

    uint64_t entry = arch_mm_table_read(root, index_of(vaddr, MM_LEVEL_PML4));
    arch_mm_entry_flags(entry, MM_LEVEL_PML4, &f);
    if (!f.present || !arch_mm_entry_is_table(entry, MM_LEVEL_PML4))
        return false;

    entry = arch_mm_table_read(arch_mm_entry_phys(entry), index_of(vaddr, MM_LEVEL_PDPT));
    arch_mm_entry_flags(entry, MM_LEVEL_PDPT, &f);
    if (!f.present)
        return false;
    if (!arch_mm_entry_is_table(entry, MM_LEVEL_PDPT)) {
        *paddr_out = arch_mm_entry_leaf_phys(entry, MM_LEVEL_PDPT) +
                     (vaddr & ((1ULL << ARCH_MM4_SHIFT_L3) - 1ULL));
        return true;
    }

    entry = arch_mm_table_read(arch_mm_entry_phys(entry), index_of(vaddr, MM_LEVEL_PD));
    arch_mm_entry_flags(entry, MM_LEVEL_PD, &f);
    if (!f.present)
        return false;
    if (!arch_mm_entry_is_table(entry, MM_LEVEL_PD)) {
        *paddr_out = arch_mm_entry_leaf_phys(entry, MM_LEVEL_PD) +
                     (vaddr & ((1ULL << ARCH_MM4_SHIFT_L2) - 1ULL));
        return true;
    }

    entry = arch_mm_table_read(arch_mm_entry_phys(entry), index_of(vaddr, MM_LEVEL_PT));
    arch_mm_entry_flags(entry, MM_LEVEL_PT, &f);
    if (!f.present)
        return false;

    *paddr_out = arch_mm_entry_leaf_phys(entry, MM_LEVEL_PT) +
                 (vaddr & ((1ULL << ARCH_MM4_SHIFT_L1) - 1ULL));
    return true;
}

void mm_query_mapping(uint64_t root, uint64_t vaddr, struct mm_query *out)
{
    out->present = false;
    out->leaf = false;
    out->level = 0;
    out->entry = 0;

    struct arch_mm_entry_flags f;
    uint64_t entry = arch_mm_table_read(root, index_of(vaddr, MM_LEVEL_PML4));
    arch_mm_entry_flags(entry, MM_LEVEL_PML4, &f);
    if (!f.present)
        return;
    if (!arch_mm_entry_is_table(entry, MM_LEVEL_PML4)) {
        /* PS is reserved in a PML4E; report the malformed entry. */
        out->present = true;
        out->level = MM_LEVEL_PML4;
        out->entry = entry;
        return;
    }

    entry = arch_mm_table_read(arch_mm_entry_phys(entry), index_of(vaddr, MM_LEVEL_PDPT));
    arch_mm_entry_flags(entry, MM_LEVEL_PDPT, &f);
    if (!f.present)
        return;
    if (!arch_mm_entry_is_table(entry, MM_LEVEL_PDPT)) {
        out->present = true;
        out->leaf = true;
        out->level = MM_LEVEL_PDPT;
        out->entry = entry;
        return;
    }

    entry = arch_mm_table_read(arch_mm_entry_phys(entry), index_of(vaddr, MM_LEVEL_PD));
    arch_mm_entry_flags(entry, MM_LEVEL_PD, &f);
    if (!f.present)
        return;
    if (!arch_mm_entry_is_table(entry, MM_LEVEL_PD)) {
        out->present = true;
        out->leaf = true;
        out->level = MM_LEVEL_PD;
        out->entry = entry;
        return;
    }

    entry = arch_mm_table_read(arch_mm_entry_phys(entry), index_of(vaddr, MM_LEVEL_PT));
    arch_mm_entry_flags(entry, MM_LEVEL_PT, &f);
    if (!f.present)
        return;

    out->present = true;
    out->leaf = true;
    out->level = MM_LEVEL_PT;
    out->entry = entry;
}

/* True when a table page has no present entry at all. */
static bool table_empty(uint64_t table_phys)
{
    for (unsigned i = 0; i < ARCH_MM4_TABLE_ENTRIES; i++) {
        if (arch_mm_entry_present(arch_mm_table_read(table_phys, i)))
            return false;
    }
    return true;
}

bool mm_clear_leaf(uint64_t root, uint64_t vaddr, bool allow_free, size_t *counter)
{
    /* Resolve parent chain. */
    unsigned l4 = index_of(vaddr, MM_LEVEL_PML4);
    uint64_t pml4e = arch_mm_table_read(root, l4);
    if (!arch_mm_entry_is_table(pml4e, MM_LEVEL_PML4))
        return false;
    uint64_t pdpt_table = arch_mm_entry_phys(pml4e);

    unsigned l3 = index_of(vaddr, MM_LEVEL_PDPT);
    uint64_t pdpte = arch_mm_table_read(pdpt_table, l3);
    if (!arch_mm_entry_is_table(pdpte, MM_LEVEL_PDPT))
        return false;
    uint64_t pd_table = arch_mm_entry_phys(pdpte);

    unsigned l2 = index_of(vaddr, MM_LEVEL_PD);
    uint64_t pde = arch_mm_table_read(pd_table, l2);
    if (!arch_mm_entry_is_table(pde, MM_LEVEL_PD))
        return false;
    uint64_t pt_table = arch_mm_entry_phys(pde);

    unsigned l1 = index_of(vaddr, MM_LEVEL_PT);
    if (!arch_mm_entry_present(arch_mm_table_read(pt_table, l1)))
        return false;

    arch_mm_table_write(pt_table, l1, 0);
    arch_mm_invlpg(vaddr);

    if (!allow_free)
        return true;

    /*
     * Release empty subtrees upwards (fixed depth): PT -> PD -> PDPT. The
     * root (PML4) stays owned by the caller.
     */
    if (!table_empty(pt_table))
        return true;

    arch_mm_table_write(pd_table, l2, 0);
    mm_free_table(pt_table, counter);

    if (!table_empty(pd_table))
        return true;

    arch_mm_table_write(pdpt_table, l3, 0);
    mm_free_table(pd_table, counter);

    if (!table_empty(pdpt_table))
        return true;

    arch_mm_table_write(root, l4, 0);
    mm_free_table(pdpt_table, counter);
    return true;
}

void mm_free_subtree(uint64_t table, unsigned level, size_t *counter)
{
    ASSERT_ALWAYS(level >= MM_LEVEL_PT && level <= MM_LEVEL_PDPT);

    for (unsigned i = 0; i < ARCH_MM4_TABLE_ENTRIES; i++) {
        uint64_t entry = arch_mm_table_read(table, i);
        struct arch_mm_entry_flags f;
        arch_mm_entry_flags(entry, level, &f);
        if (!f.present)
            continue;

        if (arch_mm_entry_is_table(entry, level)) {
            ASSERT_ALWAYS(level > MM_LEVEL_PT);
            mm_free_subtree(arch_mm_entry_phys(entry), level - 1, counter);
        }
        /* A present non-table entry is a large leaf; its frame is not ours. */
    }

    mm_free_table(table, counter);
}

bool mm_large_leaf_intersects_image(uint64_t vaddr, unsigned level,
                                    const struct arch_image_ranges *ranges)
{
    if (!ranges || (level != MM_LEVEL_PDPT && level != MM_LEVEL_PD))
        return false;
    if (ranges->data_end <= ranges->text_start)
        return false;

    uint64_t size = 1ULL << shift_for_level(level);
    uint64_t end = vaddr + size;
    if (end < vaddr)
        end = UINT64_MAX;

    return vaddr < (uint64_t)ranges->data_end && end > (uint64_t)ranges->text_start;
}

static bool preflight_spec_matches(const struct mm_preflight_spec *spec,
                                   const struct mm_query *actual)
{
    if (spec->expect == MM_EXPECT_UNMAPPED)
        return !actual->present;

    if (!actual->present || !actual->leaf || actual->level < MM_LEVEL_PT ||
        actual->level > MM_LEVEL_PDPT)
        return false;

    struct arch_mm_entry_flags f;
    arch_mm_entry_flags(actual->entry, actual->level, &f);
    if (f.user)
        return false;

    switch (spec->expect) {
    case MM_EXPECT_RX:
        return f.exec && !f.write;
    case MM_EXPECT_RW_NX:
        return f.write && !f.exec;
    case MM_EXPECT_R_NX:
        return !f.exec;
    case MM_EXPECT_UNMAPPED:
        return !actual->present;
    default:
        return false;
    }
}

bool mm_check_mappings(uint64_t root, const struct mm_preflight_spec *specs, size_t count,
                       struct mm_preflight_failure *failure)
{
    for (size_t i = 0; i < count; i++) {
        struct mm_query actual;
        mm_query_mapping(root, specs[i].vaddr, &actual);
        if (!preflight_spec_matches(&specs[i], &actual)) {
            if (failure) {
                failure->name = specs[i].name;
                failure->vaddr = specs[i].vaddr;
                failure->expect = specs[i].expect;
                failure->actual = actual;
            }
            return false;
        }
    }
    return true;
}
