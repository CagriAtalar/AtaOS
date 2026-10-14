/*
 * bitmap.h - pure bit array manipulation and search operations.
 *
 * Freestanding, architecture-independent, and host-testable under ASan/UBSan.
 * Bits represent resource allocation state:
 *   1 = allocated / reserved / used
 *   0 = free
 *
 * Scanning operations use 64-bit word comparisons and ctz64() to achieve
 * O(frames / 64) search performance (ADR-0006).
 */
#ifndef ATAOS_KERNEL_BITMAP_H
#define ATAOS_KERNEL_BITMAP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BITMAP_BIT_USED 1
#define BITMAP_BIT_FREE 0

struct bitmap {
    uint64_t *words;
    size_t bit_count;
};

/*
 * Returns the number of bytes required to hold `bit_count` bits,
 * rounded up to 64-bit word alignment.
 */
static inline size_t bitmap_byte_size(size_t bit_count)
{
    return ((bit_count + 63) / 64) * sizeof(uint64_t);
}

/*
 * Initialise a bitmap structure over caller-provided buffer `storage`.
 * If `set_all` is true, all bits are marked 1 (used).
 * If `set_all` is false, all bits are marked 0 (free), and any padding bits
 * in the final 64-bit word are set to 1 (used) to prevent out-of-bounds allocation.
 */
void bitmap_init(struct bitmap *bm, void *storage, size_t bit_count, bool set_all);

/* Mark a bit as 1 (used). Out-of-bounds bit index is safely ignored. */
void bitmap_set(struct bitmap *bm, size_t bit);

/* Mark a bit as 0 (free). Out-of-bounds bit index is safely ignored. */
void bitmap_clear(struct bitmap *bm, size_t bit);

/* Return true if bit is 1 (used). Out-of-bounds returns true (treated as used). */
bool bitmap_test(const struct bitmap *bm, size_t bit);

/* Mark a range of bits [start, start + count) as 1 (used). */
void bitmap_set_range(struct bitmap *bm, size_t start, size_t count);

/* Mark a range of bits [start, start + count) as 0 (free). */
void bitmap_clear_range(struct bitmap *bm, size_t start, size_t count);

/*
 * Find the first free bit (0) in the bitmap starting search from `*hint`.
 * If a free bit is found, updates `*hint` to `found_bit + 1` (or wraps)
 * and returns `found_bit`.
 * If no bit is free, returns `SIZE_MAX`.
 */
size_t bitmap_find_first_free(const struct bitmap *bm, size_t *hint);

/*
 * Find `count` contiguous free bits (0s) in the bitmap.
 * If found, updates `*hint` to `found_bit + count` and returns `found_bit`.
 * If no suitable run exists, returns `SIZE_MAX`.
 */
size_t bitmap_find_contiguous_free(const struct bitmap *bm, size_t count, size_t *hint);

#endif /* ATAOS_KERNEL_BITMAP_H */
