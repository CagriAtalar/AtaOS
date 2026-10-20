/*
 * test_aspace.c - host tests for address-space lifecycle and frame accounting.
 */
#include "fake_arch.h"
#include "fake_pmm.h"
#include "test_framework.h"

#include <kernel/arch.h>
#include <kernel/aspace.h>
#include <kernel/errno.h>
#include <kernel/heap.h>
#include <kernel/mm_types.h>
#include <kernel/pmm.h>

#include "aspace_internal.h"
#include "mm_internal.h"

static uint8_t s_heap_lifecycle[64 * 1024];
static uint8_t s_heap_oom[64 * 1024];

static uint64_t new_table(void)
{
    uint64_t table = pmm_alloc_frame();
    ASSERT_TRUE(table != PADDR_INVALID);
    arch_mm_table_zero(table);
    return table;
}

static bool test_aspace_lifecycle(void)
{
    fake_pmm_reset();
    fake_arch_reset();
    ASSERT_TRUE(heap_add_arena(s_heap_lifecycle, sizeof(s_heap_lifecycle)));

    uint64_t master = new_table();
    uint64_t shared = new_table();
    arch_mm_table_write(master, 256, arch_mm_entry_table(shared, false));
    size_t baseline = fake_pmm_allocated();

    struct aspace *as = NULL;
    ASSERT_EQ(aspace_create_from(master, &as), 0);
    ASSERT_TRUE(as != NULL);
    ASSERT_TRUE(aspace_pml4(as) != 0);
    ASSERT_EQ(aspace_table_frames(as), 1);

    /* User half is private and empty; kernel half is shared. */
    for (unsigned i = 0; i < 256; i++)
        ASSERT_EQ(arch_mm_table_read(aspace_pml4(as), i), 0);
    ASSERT_EQ(arch_mm_table_read(aspace_pml4(as), 256),
              arch_mm_table_read(master, 256));

    uintptr_t frame = pmm_alloc_frame();
    ASSERT_TRUE(frame != PADDR_INVALID && frame != 0);
    const uintptr_t uva = 0x0000000000400000ULL;

    ASSERT_EQ(aspace_map(as, uva, frame, 1, VMM_WRITE | VMM_USER), 0);
    ASSERT_EQ(aspace_table_frames(as), 4);

    uint64_t resolved = 0;
    ASSERT_TRUE(aspace_resolve(as, uva + 0x321, &resolved));
    ASSERT_EQ(resolved, (uint64_t)frame + 0x321);

    struct mm_query q;
    mm_query_mapping(aspace_pml4(as), uva, &q);
    struct arch_mm_entry_flags f;
    arch_mm_entry_flags(q.entry, q.level, &f);
    ASSERT_TRUE(q.present && q.leaf && f.user);
    ASSERT_TRUE(f.write && !f.exec);

    ASSERT_EQ(aspace_map(as, uva, frame, 1, VMM_WRITE | VMM_USER), -EEXIST);
    ASSERT_EQ(aspace_map(as, uva + PAGE_SIZE, frame, 1,
                         VMM_WRITE | VMM_EXEC | VMM_USER),
              -EINVAL);
    ASSERT_EQ(aspace_map(as, uva + PAGE_SIZE, frame, 1, VMM_WRITE), -EINVAL);
    ASSERT_EQ(aspace_map(as, VMM_KERN_HALF_BASE, frame, 1,
                         VMM_WRITE | VMM_USER),
              -EINVAL);
    ASSERT_EQ(aspace_map(as, uva + 1, frame, 1, VMM_WRITE | VMM_USER), -EINVAL);
    ASSERT_EQ(aspace_map(as, VMM_USER_TOP - PAGE_SIZE, frame, 2,
                         VMM_WRITE | VMM_USER),
              -EINVAL);

    ASSERT_EQ(aspace_unmap(as, uva, 1), 0);
    ASSERT_FALSE(aspace_resolve(as, uva, &resolved));
    ASSERT_EQ(aspace_unmap(as, uva, 1), -ENOENT);
    ASSERT_EQ(aspace_table_frames(as), 1);

    pmm_free_frame(frame);
    aspace_destroy(as);
    ASSERT_EQ(fake_pmm_allocated(), baseline);

    return true;
}

static bool test_aspace_create_oom(void)
{
    fake_pmm_reset();
    fake_arch_reset();
    ASSERT_TRUE(heap_add_arena(s_heap_oom, sizeof(s_heap_oom)));

    uint64_t master = new_table();
    size_t baseline = fake_pmm_allocated();
    fake_pmm_fail_after(1); /* master consumed the only allowed allocation */

    struct aspace *as = NULL;
    ASSERT_EQ(aspace_create_from(master, &as), -ENOMEM);
    ASSERT_TRUE(as == NULL);
    ASSERT_EQ(fake_pmm_allocated(), baseline);
    return true;
}

void test_aspace_suite(void)
{
    static const test_case_t tests[] = {
        {"aspace lifecycle and frame accounting", test_aspace_lifecycle},
        {"aspace creation reports OOM", test_aspace_create_oom},
    };

    run_test_suite("kernel/mm/aspace.c", tests, sizeof(tests) / sizeof(tests[0]));
}
