#include "test_framework.h"
#include <kernel/heap.h>
#include <stdint.h>
#include <string.h>

static uint8_t s_test_arena_buf[64 * 1024];

static bool test_heap_init_and_alignment(void)
{
    ASSERT_TRUE(heap_add_arena(s_test_arena_buf, sizeof(s_test_arena_buf)));

    struct heap_stats before;
    heap_get_stats(&before);
    ASSERT_TRUE(before.total_bytes > 0);
    ASSERT_TRUE(before.free_bytes > 0);
    ASSERT_EQ(before.allocated_bytes, 0);
    ASSERT_EQ(before.allocation_count, 0);

    /* Allocate various sizes and verify 16-byte alignment */
    void *p1 = kmalloc(1);
    ASSERT_TRUE(p1 != NULL);
    ASSERT_EQ((uintptr_t)p1 % 16, 0);

    void *p2 = kmalloc(17);
    ASSERT_TRUE(p2 != NULL);
    ASSERT_EQ((uintptr_t)p2 % 16, 0);

    void *p3 = kmalloc(128);
    ASSERT_TRUE(p3 != NULL);
    ASSERT_EQ((uintptr_t)p3 % 16, 0);

    struct heap_stats during;
    heap_get_stats(&during);
    ASSERT_EQ(during.allocation_count, 3);
    ASSERT_TRUE(during.allocated_bytes >= 1 + 17 + 128);

    kfree(p1);
    kfree(p2);
    kfree(p3);

    struct heap_stats after;
    heap_get_stats(&after);
    ASSERT_EQ(after.allocation_count, 0);
    ASSERT_EQ(after.allocated_bytes, 0);
    ASSERT_EQ(after.free_bytes, before.free_bytes);

    return true;
}

static bool test_heap_kcalloc_and_overflow(void)
{
    /* Reject zero allocations */
    ASSERT_TRUE(kmalloc(0) == NULL);
    ASSERT_TRUE(kcalloc(0, 10) == NULL);
    ASSERT_TRUE(kcalloc(10, 0) == NULL);

    /* Reject multiplication overflow */
    ASSERT_TRUE(kcalloc(UINT64_MAX, 2) == NULL);
    ASSERT_TRUE(kcalloc(UINT64_MAX / 2, 4) == NULL);
    ASSERT_TRUE(kcalloc(1ULL << 33, 1ULL << 33) == NULL);

    /* Normal kcalloc should be zero-initialized */
    size_t count = 32;
    uint32_t *arr = (uint32_t *)kcalloc(count, sizeof(uint32_t));
    ASSERT_TRUE(arr != NULL);
    ASSERT_EQ((uintptr_t)arr % 16, 0);

    for (size_t i = 0; i < count; i++) {
        ASSERT_EQ(arr[i], 0);
    }

    kfree(arr);

    /* kfree(NULL) safe no-op */
    kfree(NULL);

    return true;
}

static bool test_heap_split_and_coalesce(void)
{
    struct heap_stats initial;
    heap_get_stats(&initial);

    /* Allocate two blocks A and B */
    void *a = kmalloc(256);
    void *b = kmalloc(256);
    ASSERT_TRUE(a != NULL && b != NULL);

    /* Write test data */
    memset(a, 0xAA, 256);
    memset(b, 0xBB, 256);

    /* Free in reverse order and verify coalescing */
    kfree(b);
    kfree(a);

    struct heap_stats recovered;
    heap_get_stats(&recovered);
    ASSERT_EQ(recovered.allocation_count, initial.allocation_count);
    ASSERT_EQ(recovered.allocated_bytes, initial.allocated_bytes);
    ASSERT_EQ(recovered.free_bytes, initial.free_bytes);

    /* Allocate larger block that requires the coalesced chunk */
    void *large = kmalloc(512);
    ASSERT_TRUE(large != NULL);
    memset(large, 0xCC, 512);
    kfree(large);

    heap_get_stats(&recovered);
    ASSERT_EQ(recovered.free_bytes, initial.free_bytes);

    return true;
}

static bool test_heap_stress_and_leak_free(void)
{
    struct heap_stats initial;
    heap_get_stats(&initial);

    #define NUM_SLOTS 64
    void *ptrs[NUM_SLOTS];

    for (size_t i = 0; i < NUM_SLOTS; i++) {
        size_t sz = ((i * 13) % 256) + 1;
        ptrs[i] = kmalloc(sz);
        ASSERT_TRUE(ptrs[i] != NULL);
        memset(ptrs[i], (int)(i & 0xFF), sz);
    }

    /* Free half (even indices) */
    for (size_t i = 0; i < NUM_SLOTS; i += 2) {
        kfree(ptrs[i]);
        ptrs[i] = NULL;
    }

    /* Allocate new blocks in the holes */
    for (size_t i = 0; i < NUM_SLOTS; i += 2) {
        ptrs[i] = kmalloc(32);
        ASSERT_TRUE(ptrs[i] != NULL);
        memset(ptrs[i], 0x77, 32);
    }

    /* Free everything */
    for (size_t i = 0; i < NUM_SLOTS; i++) {
        kfree(ptrs[i]);
        ptrs[i] = NULL;
    }

    struct heap_stats final_stats;
    heap_get_stats(&final_stats);

    ASSERT_EQ(final_stats.allocation_count, initial.allocation_count);
    ASSERT_EQ(final_stats.allocated_bytes, initial.allocated_bytes);
    ASSERT_EQ(final_stats.free_bytes, initial.free_bytes);

    return true;
}

void test_heap_suite(void)
{
    static const test_case_t tests[] = {
        {"heap arena init and 16-byte alignment", test_heap_init_and_alignment},
        {"kcalloc zero-init, overflow rejection, and kfree(NULL)", test_heap_kcalloc_and_overflow},
        {"heap split, coalesce, and reuse", test_heap_split_and_coalesce},
        {"heap fragmentation stress test and leak-free reclamation", test_heap_stress_and_leak_free},
    };
    run_test_suite("kernel/mm/heap.c", tests, sizeof(tests) / sizeof(tests[0]));
}
