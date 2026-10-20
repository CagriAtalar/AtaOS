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
#include <kernel/mm_types.h>
#include <kernel/pmm.h>

#include "mm_internal.h"

static uint64_t new_root(void)
{
    uint64_t root = pmm_alloc_frame();
    ASSERT_TRUE(root != PADDR_INVALID);
    arch_mm_table_zero(root);
    return root;
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

void test_mm_suite(void)
{
    static const test_case_t tests[] = {
        {"canonical PML4 bases", test_canonical_bases},
        {"large leaf physical address ignores PAT", test_large_leaf_physical_address_ignores_pat},
        {"reprotect preserves non-permission bits", test_reprotect_preserves_non_permission_bits},
        {"translate returns exact 4 KiB address", test_translate_returns_exact_4k_address},
        {"set/clear leaf overlap and absent", test_set_leaf_rejects_overlap_and_clear_reports_absent},
    };

    run_test_suite("mm", tests, sizeof(tests) / sizeof(tests[0]));
}
