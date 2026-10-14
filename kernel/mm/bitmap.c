/*
 * bitmap.c - pure bit array manipulation and search operations.
 *
 * Implements bitmap frame allocation logic (ADR-0006).
 * Freestanding, architecture-independent, and host-testable under ASan/UBSan.
 */
#include "kernel/bitmap.h"
#include "kernel/bits.h"
#include "kernel/string.h"

void bitmap_init(struct bitmap *bm, void *storage, size_t bit_count, bool set_all)
{
    if (!bm)
        return;

    bm->words = (uint64_t *)storage;
    bm->bit_count = bit_count;

    if (!storage || bit_count == 0)
        return;

    size_t num_words = (bit_count + 63) / 64;
    size_t byte_size = num_words * sizeof(uint64_t);

    if (set_all) {
        memset(storage, 0xFF, byte_size);
    } else {
        memset(storage, 0x00, byte_size);
        /* If bit_count does not end on a 64-bit boundary, mark the padding bits
         * in the last word as used (1) so scans never return an out-of-bounds bit. */
        size_t rem = bit_count % 64;
        if (rem != 0) {
            uint64_t pad_mask = ~((1ULL << rem) - 1);
            bm->words[num_words - 1] |= pad_mask;
        }
    }
}

void bitmap_set(struct bitmap *bm, size_t bit)
{
    if (!bm || !bm->words || bit >= bm->bit_count)
        return;

    bm->words[bit / 64] |= (1ULL << (bit % 64));
}

void bitmap_clear(struct bitmap *bm, size_t bit)
{
    if (!bm || !bm->words || bit >= bm->bit_count)
        return;

    bm->words[bit / 64] &= ~(1ULL << (bit % 64));
}

bool bitmap_test(const struct bitmap *bm, size_t bit)
{
    if (!bm || !bm->words || bit >= bm->bit_count)
        return true; /* Out of bounds is treated as allocated/reserved */

    return (bm->words[bit / 64] & (1ULL << (bit % 64))) != 0;
}

void bitmap_set_range(struct bitmap *bm, size_t start, size_t count)
{
    if (!bm || !bm->words || count == 0 || start >= bm->bit_count)
        return;

    size_t end;
    if (__builtin_add_overflow(start, count, &end) || end > bm->bit_count)
        end = bm->bit_count;

    size_t i = start;
    /* Head bits up to 64-bit word boundary */
    while (i < end && (i % 64) != 0) {
        bm->words[i / 64] |= (1ULL << (i % 64));
        i++;
    }
    /* Full 64-bit words */
    while (i + 64 <= end) {
        bm->words[i / 64] = UINT64_MAX;
        i += 64;
    }
    /* Tail bits */
    while (i < end) {
        bm->words[i / 64] |= (1ULL << (i % 64));
        i++;
    }
}

void bitmap_clear_range(struct bitmap *bm, size_t start, size_t count)
{
    if (!bm || !bm->words || count == 0 || start >= bm->bit_count)
        return;

    size_t end;
    if (__builtin_add_overflow(start, count, &end) || end > bm->bit_count)
        end = bm->bit_count;

    size_t i = start;
    /* Head bits up to 64-bit word boundary */
    while (i < end && (i % 64) != 0) {
        bm->words[i / 64] &= ~(1ULL << (i % 64));
        i++;
    }
    /* Full 64-bit words */
    while (i + 64 <= end) {
        bm->words[i / 64] = 0ULL;
        i += 64;
    }
    /* Tail bits */
    while (i < end) {
        bm->words[i / 64] &= ~(1ULL << (i % 64));
        i++;
    }
}

/*
 * Search for the first free bit (0) in range [start, end).
 * Returns the bit index if found, or SIZE_MAX if none found.
 */
static size_t find_free_in_range(const struct bitmap *bm, size_t start, size_t end)
{
    if (start >= end || start >= bm->bit_count)
        return SIZE_MAX;

    if (end > bm->bit_count)
        end = bm->bit_count;

    size_t start_word = start / 64;
    size_t end_word = (end + 63) / 64;

    for (size_t w = start_word; w < end_word; w++) {
        uint64_t free_bits = ~bm->words[w];
        if (free_bits == 0)
            continue; /* All 64 bits in this word are used */

        /* Mask off bits below start */
        if (w == start / 64) {
            uint64_t start_mask = ~((1ULL << (start % 64)) - 1);
            free_bits &= start_mask;
        }

        /* Mask off bits at or above end */
        if (w == (end - 1) / 64) {
            size_t end_rem = end % 64;
            if (end_rem != 0) {
                uint64_t end_mask = (1ULL << end_rem) - 1;
                free_bits &= end_mask;
            }
        }

        if (free_bits != 0) {
            unsigned bit_offset = ctz64(free_bits);
            size_t found = w * 64 + bit_offset;
            if (found < end)
                return found;
        }
    }

    return SIZE_MAX;
}

size_t bitmap_find_first_free(const struct bitmap *bm, size_t *hint)
{
    if (!bm || !bm->words || bm->bit_count == 0)
        return SIZE_MAX;

    size_t start = 0;
    if (hint && *hint < bm->bit_count)
        start = *hint;

    /* Search from hint to end of bitmap */
    size_t found = find_free_in_range(bm, start, bm->bit_count);
    if (found != SIZE_MAX) {
        if (hint)
            *hint = (found + 1 < bm->bit_count) ? (found + 1) : 0;
        return found;
    }

    /* Wrap around and search from 0 to hint */
    if (start > 0) {
        found = find_free_in_range(bm, 0, start);
        if (found != SIZE_MAX) {
            if (hint)
                *hint = (found + 1 < bm->bit_count) ? (found + 1) : 0;
            return found;
        }
    }

    return SIZE_MAX;
}

size_t bitmap_find_contiguous_free(const struct bitmap *bm, size_t count, size_t *hint)
{
    if (!bm || !bm->words || count == 0 || count > bm->bit_count)
        return SIZE_MAX;

    if (count == 1)
        return bitmap_find_first_free(bm, hint);

    size_t start = 0;
    if (hint && *hint <= bm->bit_count - count)
        start = *hint;

    for (size_t pass = 0; pass < 2; pass++) {
        size_t run_start = start;
        size_t run_len = 0;

        for (size_t i = start; i < bm->bit_count; i++) {
            /* Fast-skip 64 used bits when not in an active run */
            if (run_len == 0 && (i % 64 == 0) && (i + 64 <= bm->bit_count) &&
                bm->words[i / 64] == UINT64_MAX) {
                i += 63;
                continue;
            }

            if (!bitmap_test(bm, i)) {
                if (run_len == 0)
                    run_start = i;
                run_len++;
                if (run_len == count) {
                    if (hint)
                        *hint = (run_start + count < bm->bit_count) ? (run_start + count) : 0;
                    return run_start;
                }
            } else {
                run_len = 0;
            }
        }

        if (start == 0)
            break;

        /* Wrap-around for second pass */
        start = 0;
    }

    return SIZE_MAX;
}
