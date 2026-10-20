/*
 * test_mm.c - host tests for page-table encoding and kernel/mm walkers.
 *
 * The same pure encoding helpers used by the kernel are compiled here against
 * the fake architecture and fake PMM. This makes canonical-address bugs,
 * large-page PAT handling and permission preservation reproducible without
 * booting a CPU.
 */
#include "fake_arch.h"
#include "fake_pmm.h"
#include "test_framework.h"

#include <arch/paging.h>
#include <kernel/arch.h>
#include <kernel/errno.h>
#include <kernel/mm_types.h>
#include <kernel/pmm.h>
#include <kernel/vmm.h>

#include "mm_internal.h"
#include "vmm_internal.h"

static uint64_t new_table(void)
{
    uint64_t table = pmm_alloc_frame();
    ASSERT_TRUE(table != PADDR_INVALID);
    arch_mm_table_zero(table);
    return table;
}

static uint64_t new_root(void)
{
    return new_table();
}

static unsigned index_at(uint64_t vaddr, unsigned level)
{
    unsigned shift = level == 4 ? 39U : level == 3 ? 30U : level == 2 ? 21U : 12U;
    return (unsigned)((vaddr >> shift) & 511U);
}

static bool test_canonical_bases(void)
{
    ASSERT_EQ(mm_canonical_base(0), 0x0000000000000000ULL);
    ASSERT_EQ(mm_canonical_base(255), 0x00007f8000000000ULL);
    ASSERT_EQ(mm_canonical_base(256), 0xffff800000000000ULL);
    ASSERT_EQ(mm_canonical_base(511), 0xffffff8000000000ULL);
    return true;
}

static bool test_large_leaf_physical_address_ignores_pat(void)
{
    const uint64_t frame = 0x0000000000400000ULL;
    uint64_t entry = arch_mm_entry_leaf(frame | (1ULL << 12), true, false, false, true);

    ASSERT_EQ(arch_mm_entry_leaf_phys(entry, MM_LEVEL_PD), frame);
    return true;
}

static bool test_reprotect_preserves_non_permission_bits(void)
{
    const uint64_t frame = 0x0000000012345000ULL;
    uint64_t entry = MM_PTE_PRESENT | MM_PTE_WRITE | MM_PTE_PWT | MM_PTE_PCD | (1ULL << 7) |
                     MM_PTE_G | (1ULL << 9) | frame;

    uint64_t out = arch_mm_leaf_reprotect(entry, false, true, false);

    ASSERT_TRUE((out & MM_PTE_PWT) != 0);
    ASSERT_TRUE((out & MM_PTE_PCD) != 0);
    ASSERT_TRUE((out & (1ULL << 7)) != 0); /* 4 KiB PAT, not a PS bit here */
    ASSERT_TRUE((out & MM_PTE_G) != 0);
    ASSERT_TRUE((out & (1ULL << 9)) != 0); /* AVL */
    ASSERT_FALSE((out & MM_PTE_WRITE) != 0);
    ASSERT_FALSE((out & MM_PTE_NX) != 0);
    ASSERT_EQ(out & MM_ADDR_MASK, frame);
    return true;
}

static bool test_translate_returns_exact_4k_address(void)
{
    fake_pmm_reset();
    fake_arch_reset();

    uint64_t root = new_root();
    uint64_t frame = pmm_alloc_frame();
    ASSERT_TRUE(frame != PADDR_INVALID);

    const uint64_t vaddr = 0xffffc00000001000ULL;
    uint64_t pt = 0;
    ASSERT_EQ(mm_set_leaf(root, vaddr, arch_mm_entry_leaf(frame, true, false, false, false),
                          false, NULL, &pt),
              MM_SET_OK);

    uint64_t resolved = 0;
    ASSERT_TRUE(mm_translate(root, vaddr + 0x123, &resolved));
    ASSERT_EQ(resolved, frame + 0x123);
    return true;
}

static bool test_set_leaf_rejects_overlap_and_clear_reports_absent(void)
{
    fake_pmm_reset();
    fake_arch_reset();

    uint64_t root = new_root();
    uint64_t frame = pmm_alloc_frame();
    ASSERT_TRUE(frame != PADDR_INVALID);
    const uint64_t vaddr = 0xffffc00000002000ULL;
    uint64_t pt = 0;

    ASSERT_EQ(mm_set_leaf(root, vaddr, arch_mm_entry_leaf(frame, true, false, false, false),
                          false, NULL, &pt),
              MM_SET_OK);
    ASSERT_EQ(mm_set_leaf(root, vaddr, arch_mm_entry_leaf(frame, true, false, false, false),
                          false, NULL, &pt),
              MM_SET_EXISTS);
    ASSERT_TRUE(mm_clear_leaf(root, vaddr, false, NULL));
    ASSERT_FALSE(mm_clear_leaf(root, vaddr, false, NULL));
    return true;
}

static bool test_entry_flags_are_level_aware(void)
{
    uint64_t pte = MM_PTE_PRESENT | MM_PTE_PAT | 0x12345000ULL;
    uint64_t pde = MM_PTE_PRESENT | MM_PTE_LARGE | (1ULL << 12) | 0x400000ULL;
    struct arch_mm_entry_flags f;

    arch_mm_entry_flags(pte, MM_LEVEL_PT, &f);
    ASSERT_FALSE(f.large); /* bit 7 is PAT in a PTE */

    arch_mm_entry_flags(pde, MM_LEVEL_PD, &f);
    ASSERT_TRUE(f.large); /* bit 7 is PS in a PDE */
    return true;
}

static bool test_translate_exact_2m_address(void)
{
    fake_pmm_reset();
    fake_arch_reset();

    uint64_t root = new_root();
    uint64_t pdpt = new_table();
    uint64_t pd = new_table();
    const uint64_t vaddr = 0xffff800000000000ULL;
    const uint64_t frame = 0x0000000000400000ULL;
    uint64_t leaf = arch_mm_entry_leaf(frame | (1ULL << 12), false, false, false, true);

    arch_mm_table_write(root, index_at(vaddr, 4), arch_mm_entry_table(pdpt, false));
    arch_mm_table_write(pdpt, index_at(vaddr, 3), arch_mm_entry_table(pd, false));
    arch_mm_table_write(pd, index_at(vaddr, 2), leaf);

    uint64_t resolved = 0;
    ASSERT_TRUE(mm_translate(root, vaddr + 0x1234, &resolved));
    ASSERT_EQ(resolved, frame + 0x1234);

    struct mm_query q;
    mm_query_mapping(root, vaddr + 0x1234, &q);
    ASSERT_TRUE(q.present);
    ASSERT_TRUE(q.leaf);
    ASSERT_EQ(q.level, MM_LEVEL_PD);
    return true;
}

static bool test_translate_exact_1g_address(void)
{
    fake_pmm_reset();
    fake_arch_reset();

    uint64_t root = new_root();
    uint64_t pdpt = new_table();
    const uint64_t vaddr = 0xffff800000000000ULL;
    const uint64_t frame = 0x0000000040000000ULL;
    uint64_t leaf = arch_mm_entry_leaf(frame | (1ULL << 12), false, false, false, true);

    arch_mm_table_write(root, index_at(vaddr, 4), arch_mm_entry_table(pdpt, false));
    arch_mm_table_write(pdpt, index_at(vaddr, 3), leaf);

    uint64_t resolved = 0;
    ASSERT_TRUE(mm_translate(root, vaddr + 0x123456, &resolved));
    ASSERT_EQ(resolved, frame + 0x123456);

    struct mm_query q;
    mm_query_mapping(root, vaddr + 0x123456, &q);
    ASSERT_TRUE(q.present);
    ASSERT_TRUE(q.leaf);
    ASSERT_EQ(q.level, MM_LEVEL_PDPT);
    return true;
}

static bool test_reprotect_preserves_large_leaf_attributes(void)
{
    const uint64_t frame = 0x0000000040000000ULL;
    uint64_t entry = MM_PTE_PRESENT | MM_PTE_WRITE | MM_PTE_LARGE | (1ULL << 12) |
                     MM_PTE_PWT | MM_PTE_PCD | MM_PTE_G | (1ULL << 10) | frame;
    uint64_t out = arch_mm_leaf_reprotect(entry, false, false, false);

    ASSERT_TRUE((out & MM_PTE_LARGE) != 0);
    ASSERT_TRUE((out & (1ULL << 12)) != 0);
    ASSERT_TRUE((out & MM_PTE_PWT) != 0);
    ASSERT_TRUE((out & MM_PTE_PCD) != 0);
    ASSERT_TRUE((out & MM_PTE_G) != 0);
    ASSERT_TRUE((out & (1ULL << 10)) != 0);
    ASSERT_FALSE((out & MM_PTE_WRITE) != 0);
    ASSERT_TRUE((out & MM_PTE_NX) != 0);
    return true;
}

static bool test_fallible_table_allocation_unwinds(void)
{
    fake_pmm_reset();
    fake_arch_reset();

    uint64_t root = new_root();
    uint64_t frame = pmm_alloc_frame();
    ASSERT_TRUE(frame != PADDR_INVALID);
    size_t before = fake_pmm_allocated();

    /* The root and leaf frame consumed two allocations; fail before the first new table. */
    fake_pmm_fail_after(2);
    ASSERT_EQ(mm_set_leaf(root, 0xffffc00000003000ULL,
                          arch_mm_entry_leaf(frame, true, false, false, false), false, NULL,
                          NULL),
              MM_SET_NOMEM);
    ASSERT_EQ(fake_pmm_allocated(), before);

    /* Allow the PDPT, then fail while allocating the PD: unwind the PDPT. */
    fake_pmm_fail_after(3);
    ASSERT_EQ(mm_set_leaf(root, 0xffffc00000004000ULL,
                          arch_mm_entry_leaf(frame, true, false, false, false), false, NULL,
                          NULL),
              MM_SET_NOMEM);
    ASSERT_EQ(fake_pmm_allocated(), before);
    return true;
}

static bool test_large_leaf_image_intersection(void)
{
    struct arch_image_ranges r = {
        .text_start = 0xffffffff80000000ULL,
        .text_end = 0xffffffff80005000ULL,
        .rodata_start = 0xffffffff80006000ULL,
        .rodata_end = 0xffffffff80007000ULL,
        .data_start = 0xffffffff80007000ULL,
        .data_end = 0xffffffff80009000ULL,
    };

    ASSERT_TRUE(mm_large_leaf_intersects_image(0xffffffff80000000ULL, MM_LEVEL_PD, &r));
    ASSERT_FALSE(mm_large_leaf_intersects_image(0xffffffff7fe00000ULL, MM_LEVEL_PD, &r));
    ASSERT_FALSE(mm_large_leaf_intersects_image(0xffffffff80200000ULL, MM_LEVEL_PD, &r));
    ASSERT_TRUE(mm_large_leaf_intersects_image(0xffffffff80000000ULL, MM_LEVEL_PDPT, &r));
    ASSERT_FALSE(mm_large_leaf_intersects_image(0xffff800000000000ULL, MM_LEVEL_PD, &r));
    return true;
}

static bool test_preflight_checks_permissions(void)
{
    fake_pmm_reset();
    fake_arch_reset();

    uint64_t root = new_root();
    uint64_t code = pmm_alloc_frame();
    uint64_t data = pmm_alloc_frame();
    ASSERT_TRUE(code != PADDR_INVALID && data != PADDR_INVALID);

    const uint64_t code_va = 0xffffffff80000000ULL;
    const uint64_t data_va = 0xffffffff80002000ULL;
    const uint64_t absent_va = 0xffffc00000000000ULL;

    ASSERT_EQ(mm_set_leaf(root, code_va, arch_mm_entry_leaf(code, false, true, false, false),
                          false, NULL, NULL),
              MM_SET_OK);
    ASSERT_EQ(mm_set_leaf(root, data_va, arch_mm_entry_leaf(data, true, false, false, false),
                          false, NULL, NULL),
              MM_SET_OK);

    struct mm_preflight_spec specs[] = {
        {code_va, MM_EXPECT_RX, "code"},
        {data_va, MM_EXPECT_RW_NX, "data"},
        {absent_va, MM_EXPECT_UNMAPPED, "absent"},
    };
    struct mm_preflight_failure failure;
    ASSERT_TRUE(mm_check_mappings(root, specs, 3, &failure));

    specs[0].expect = MM_EXPECT_RW_NX; /* code is not writable */
    ASSERT_FALSE(mm_check_mappings(root, specs, 3, &failure));
    ASSERT_STR_EQ(failure.name, "code");
    ASSERT_EQ(failure.vaddr, code_va);
    ASSERT_TRUE(failure.actual.present);
    return true;
}

static bool test_vmm_range_validation(void)
{
    ASSERT_EQ(vmm_check_flags(0), 0);
    ASSERT_EQ(vmm_check_flags(VMM_WRITE), 0);
    ASSERT_EQ(vmm_check_flags(VMM_EXEC), 0);
    ASSERT_EQ(vmm_check_flags(VMM_WRITE | VMM_EXEC), -EINVAL);
    ASSERT_EQ(vmm_check_flags(1U << 7), -EINVAL);
    ASSERT_EQ(vmm_check_flags(VMM_USER), 0);

    ASSERT_EQ(vmm_validate_range(VMM_DYNMAP_BASE, 0x1000, 1, VMM_WRITE), 0);
    ASSERT_EQ(vmm_validate_range(VMM_DYNMAP_BASE, 0x1000, 0, VMM_WRITE), 0);
    ASSERT_EQ(vmm_validate_range(VMM_DYNMAP_BASE + 1, 0x1000, 1, VMM_WRITE), -EINVAL);
    ASSERT_EQ(vmm_validate_range(VMM_DYNMAP_BASE - PAGE_SIZE, 0x1000, 1, VMM_WRITE), -EINVAL);
    ASSERT_EQ(vmm_validate_range(VMM_DYNMAP_BASE, 0x1001, 1, VMM_WRITE), -EINVAL);
    ASSERT_EQ(vmm_validate_range(VMM_DYNMAP_BASE, 0x1000, 1, VMM_USER), -EINVAL);
    ASSERT_EQ(vmm_validate_range(VMM_DYNMAP_END - PAGE_SIZE, 0x1000, 2, VMM_WRITE), -EINVAL);
    ASSERT_EQ(vmm_validate_range(VMM_DYNMAP_BASE, 0x1000, SIZE_MAX, VMM_WRITE), -EINVAL);
    ASSERT_EQ(vmm_validate_range(VMM_DYNMAP_BASE, UINT64_MAX & ~PAGE_OFFSET_MASK, 1,
                                 VMM_WRITE),
              -EINVAL);
    return true;
}

void test_mm_suite(void)
{
    static const test_case_t tests[] = {
        {"canonical PML4 bases", test_canonical_bases},
        {"large leaf physical address ignores PAT", test_large_leaf_physical_address_ignores_pat},
        {"reprotect preserves non-permission bits", test_reprotect_preserves_non_permission_bits},
        {"reprotect preserves large leaf attributes",
         test_reprotect_preserves_large_leaf_attributes},
        {"entry flags are level-aware", test_entry_flags_are_level_aware},
        {"translate returns exact 4 KiB address", test_translate_returns_exact_4k_address},
        {"translate returns exact 2 MiB address", test_translate_exact_2m_address},
        {"translate returns exact 1 GiB address", test_translate_exact_1g_address},
        {"set/clear leaf overlap and absent", test_set_leaf_rejects_overlap_and_clear_reports_absent},
        {"fallible table allocation unwinds", test_fallible_table_allocation_unwinds},
        {"large leaf image intersection", test_large_leaf_image_intersection},
        {"pre-flight checks permissions", test_preflight_checks_permissions},
        {"vmm range and flag validation", test_vmm_range_validation},
    };

    run_test_suite("mm", tests, sizeof(tests) / sizeof(tests[0]));
}
