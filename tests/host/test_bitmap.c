#include "test_framework.h"
#include <kernel/bitmap.h>

static bool test_bitmap_init_and_basic(void)
{
    uint64_t storage[4];
    struct bitmap bm;

    ASSERT_EQ(bitmap_byte_size(100), 2 * sizeof(uint64_t));
    ASSERT_EQ(bitmap_byte_size(64), 1 * sizeof(uint64_t));
    ASSERT_EQ(bitmap_byte_size(65), 2 * sizeof(uint64_t));

    bitmap_init(&bm, storage, 100, true);
    ASSERT_EQ(bm.bit_count, 100);

    for (size_t i = 0; i < 100; i++) {
        ASSERT_TRUE(bitmap_test(&bm, i));
    }

    bitmap_clear(&bm, 0);
    bitmap_clear(&bm, 42);
    bitmap_clear(&bm, 99);

    ASSERT_FALSE(bitmap_test(&bm, 0));
    ASSERT_FALSE(bitmap_test(&bm, 42));
    ASSERT_FALSE(bitmap_test(&bm, 99));

    ASSERT_TRUE(bitmap_test(&bm, 1));
    ASSERT_TRUE(bitmap_test(&bm, 41));
    ASSERT_TRUE(bitmap_test(&bm, 43));
    ASSERT_TRUE(bitmap_test(&bm, 98));

    bitmap_set(&bm, 42);
    ASSERT_TRUE(bitmap_test(&bm, 42));

    /* Out-of-bounds safety */
    ASSERT_TRUE(bitmap_test(&bm, 100));
    ASSERT_TRUE(bitmap_test(&bm, 500));
    bitmap_set(&bm, 100);
    bitmap_clear(&bm, 100);

    return true;
}

static bool test_bitmap_padding_and_exhaustion(void)
{
    uint64_t storage[2];
    struct bitmap bm;

    /* 70 bits spans word 0 (64 bits) and word 1 (6 bits used, 58 bits padding) */
    bitmap_init(&bm, storage, 70, false);

    for (size_t i = 0; i < 70; i++) {
        ASSERT_FALSE(bitmap_test(&bm, i));
    }

    /* Padding bits 70..127 must be marked 1 (used) in word 1 */
    uint64_t pad_mask = ~((1ULL << 6) - 1);
    ASSERT_EQ(storage[1] & pad_mask, pad_mask);

    /* Allocate all 70 bits */
    for (size_t i = 0; i < 70; i++) {
        bitmap_set(&bm, i);
    }

    /* First free should now return SIZE_MAX because padding bits cannot be allocated */
    size_t hint = 0;
    ASSERT_EQ(bitmap_find_first_free(&bm, &hint), SIZE_MAX);

    return true;
}

static bool test_bitmap_find_first_free_and_wrap(void)
{
    uint64_t storage[4];
    struct bitmap bm;

    bitmap_init(&bm, storage, 200, true);

    bitmap_clear(&bm, 10);
    bitmap_clear(&bm, 50);
    bitmap_clear(&bm, 150);

    size_t hint = 0;
    size_t f1 = bitmap_find_first_free(&bm, &hint);
    ASSERT_EQ(f1, 10);
    ASSERT_EQ(hint, 11);

    bitmap_set(&bm, 10);
    size_t f2 = bitmap_find_first_free(&bm, &hint);
    ASSERT_EQ(f2, 50);
    ASSERT_EQ(hint, 51);

    bitmap_set(&bm, 50);
    size_t f3 = bitmap_find_first_free(&bm, &hint);
    ASSERT_EQ(f3, 150);
    ASSERT_EQ(hint, 151);

    /* Test wrap-around: clear bit 25 before hint */
    bitmap_clear(&bm, 25);
    size_t f4 = bitmap_find_first_free(&bm, &hint);
    ASSERT_EQ(f4, 25);
    ASSERT_EQ(hint, 26);

    bitmap_set(&bm, 25);
    bitmap_set(&bm, 150);
    ASSERT_EQ(bitmap_find_first_free(&bm, &hint), SIZE_MAX);

    return true;
}

static bool test_bitmap_range_operations(void)
{
    uint64_t storage[4];
    struct bitmap bm;

    bitmap_init(&bm, storage, 256, false);

    /* Set range spanning multiple words: bits 15 to 114 (count 100) */
    bitmap_set_range(&bm, 15, 100);

    for (size_t i = 0; i < 15; i++) {
        ASSERT_FALSE(bitmap_test(&bm, i));
    }
    for (size_t i = 15; i < 115; i++) {
        ASSERT_TRUE(bitmap_test(&bm, i));
    }
    for (size_t i = 115; i < 256; i++) {
        ASSERT_FALSE(bitmap_test(&bm, i));
    }

    /* Clear subrange: bits 50 to 79 (count 30) */
    bitmap_clear_range(&bm, 50, 30);
    for (size_t i = 15; i < 50; i++) {
        ASSERT_TRUE(bitmap_test(&bm, i));
    }
    for (size_t i = 50; i < 80; i++) {
        ASSERT_FALSE(bitmap_test(&bm, i));
    }
    for (size_t i = 80; i < 115; i++) {
        ASSERT_TRUE(bitmap_test(&bm, i));
    }

    return true;
}

static bool test_bitmap_contiguous_allocation(void)
{
    uint64_t storage[4];
    struct bitmap bm;

    bitmap_init(&bm, storage, 128, true);

    /* Hole 1: 5 free bits at 10 */
    bitmap_clear_range(&bm, 10, 5);
    /* Hole 2: 10 free bits at 30 */
    bitmap_clear_range(&bm, 30, 10);
    /* Hole 3: 20 free bits at 70 */
    bitmap_clear_range(&bm, 70, 20);

    size_t hint = 0;

    /* Request 25 -> too large */
    ASSERT_EQ(bitmap_find_contiguous_free(&bm, 25, &hint), SIZE_MAX);

    /* Request 15 -> should find hole 3 at 70 */
    ASSERT_EQ(bitmap_find_contiguous_free(&bm, 15, &hint), 70);

    /* Request 8 -> should find hole 2 at 30 */
    hint = 0;
    ASSERT_EQ(bitmap_find_contiguous_free(&bm, 8, &hint), 30);

    /* Request 4 -> should find hole 1 at 10 */
    hint = 0;
    ASSERT_EQ(bitmap_find_contiguous_free(&bm, 4, &hint), 10);

    return true;
}

void test_bitmap_suite(void)
{
    static const test_case_t tests[] = {
        {"bitmap init, set, clear, test, and bounds safety", test_bitmap_init_and_basic},
        {"bitmap non-multiple-of-64 padding and exhaustion", test_bitmap_padding_and_exhaustion},
        {"bitmap find first free with next-fit hint and wrap-around", test_bitmap_find_first_free_and_wrap},
        {"bitmap multi-word set_range and clear_range", test_bitmap_range_operations},
        {"bitmap find contiguous free runs", test_bitmap_contiguous_allocation},
    };
    run_test_suite("kernel/mm/bitmap.c", tests, sizeof(tests) / sizeof(tests[0]));
}
