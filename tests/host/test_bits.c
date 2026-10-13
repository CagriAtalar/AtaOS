#include "test_framework.h"
#include <kernel/bits.h>

static bool test_alignment_functions(void) {
    ASSERT_EQ(align_up(0, 4096), 0);
    ASSERT_EQ(align_up(1, 4096), 4096);
    ASSERT_EQ(align_up(4095, 4096), 4096);
    ASSERT_EQ(align_up(4096, 4096), 4096);
    ASSERT_EQ(align_up(4097, 4096), 8192);

    ASSERT_EQ(align_down(0, 4096), 0);
    ASSERT_EQ(align_down(1, 4096), 0);
    ASSERT_EQ(align_down(4095, 4096), 0);
    ASSERT_EQ(align_down(4096, 4096), 4096);
    ASSERT_EQ(align_down(4097, 4096), 4096);

    ASSERT_TRUE(is_aligned(0, 4096));
    ASSERT_FALSE(is_aligned(1, 4096));
    ASSERT_TRUE(is_aligned(4096, 4096));
    ASSERT_FALSE(is_aligned(4097, 4096));
    ASSERT_TRUE(is_aligned(0x100000, 4096));

    uint64_t out = 0;
    ASSERT_TRUE(align_up_checked(4095, 4096, &out));
    ASSERT_EQ(out, 4096);
    ASSERT_FALSE(align_up_checked(UINT64_MAX - 10, 4096, &out)); /* overflow */
    ASSERT_FALSE(align_up_checked(100, 7, &out)); /* non-pow2 */

    return true;
}

static bool test_bit_and_math_helpers(void) {
    ASSERT_TRUE(is_pow2(1));
    ASSERT_TRUE(is_pow2(2));
    ASSERT_TRUE(is_pow2(4096));
    ASSERT_FALSE(is_pow2(0));
    ASSERT_FALSE(is_pow2(3));
    ASSERT_FALSE(is_pow2(4095));

    ASSERT_EQ(BIT(0), 1ULL);
    ASSERT_EQ(BIT(1), 2ULL);
    ASSERT_EQ(BIT(12), 4096ULL);
    ASSERT_EQ(BIT(63), 0x8000000000000000ULL);

    ASSERT_EQ(popcount64(0), 0);
    ASSERT_EQ(popcount64(0x01), 1);
    ASSERT_EQ(popcount64(0xFF), 8);
    ASSERT_EQ(popcount64(0x8000000000000001ULL), 2);

    ASSERT_EQ(ctz64(1), 0);
    ASSERT_EQ(ctz64(2), 1);
    ASSERT_EQ(ctz64(4096), 12);

    ASSERT_EQ(ilog2_u64(1), 0);
    ASSERT_EQ(ilog2_u64(2), 1);
    ASSERT_EQ(ilog2_u64(4096), 12);
    ASSERT_EQ(ilog2_u64(8191), 12);
    ASSERT_EQ(ilog2_u64(8192), 13);

    uint64_t sum = 0;
    ASSERT_FALSE(add_overflows_u64(10, 20, &sum));
    ASSERT_EQ(sum, 30);
    ASSERT_TRUE(add_overflows_u64(UINT64_MAX, 1, &sum));

    uint64_t prod = 0;
    ASSERT_FALSE(mul_overflows_u64(10, 20, &prod));
    ASSERT_EQ(prod, 200);
    ASSERT_TRUE(mul_overflows_u64(UINT64_MAX, 2, &prod));

    return true;
}

void test_bits_suite(void) {
    static const test_case_t tests[] = {
        {"alignment helpers (align_up, align_down, is_aligned, align_up_checked)", test_alignment_functions},
        {"bit and math helpers (is_pow2, BIT, popcount, ctz, ilog2, overflow checks)", test_bit_and_math_helpers},
    };
    run_test_suite("include/kernel/bits.h", tests, sizeof(tests) / sizeof(tests[0]));
}
