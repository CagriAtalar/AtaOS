/*
 * bits.h - alignment and bit-manipulation helpers.
 *
 * All functions are pure and header-only so they can be tested on the host.
 * Alignments must be powers of two unless stated otherwise.
 */
#ifndef ATAOS_KERNEL_BITS_H
#define ATAOS_KERNEL_BITS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BIT(n) (1ULL << (n))

/* True if v is a non-zero power of two. */
static inline bool is_pow2(uint64_t v)
{
    return v != 0 && (v & (v - 1)) == 0;
}

/* Round v down to a multiple of align (align must be a power of two). */
static inline uint64_t align_down(uint64_t v, uint64_t align)
{
    return v & ~(align - 1);
}

/*
 * Round v up to a multiple of align (align must be a power of two).
 * Wraps on overflow; use align_up_checked() for untrusted values.
 */
static inline uint64_t align_up(uint64_t v, uint64_t align)
{
    return (v + (align - 1)) & ~(align - 1);
}

/*
 * Overflow-checked align_up. Returns false (and leaves *out untouched) if
 * align is not a power of two or the rounded value does not fit in 64 bits.
 */
static inline bool align_up_checked(uint64_t v, uint64_t align, uint64_t *out)
{
    if (!is_pow2(align))
        return false;
    uint64_t add = align - 1;
    if (v > UINT64_MAX - add)
        return false;
    *out = (v + add) & ~add;
    return true;
}

static inline bool is_aligned(uint64_t v, uint64_t align)
{
    return (v & (align - 1)) == 0;
}

/* Overflow-checked addition of two 64-bit values. Returns true on overflow. */
static inline bool add_overflows_u64(uint64_t a, uint64_t b, uint64_t *out)
{
    return __builtin_add_overflow(a, b, out);
}

/* Overflow-checked multiplication of two 64-bit values. Returns true on overflow. */
static inline bool mul_overflows_u64(uint64_t a, uint64_t b, uint64_t *out)
{
    return __builtin_mul_overflow(a, b, out);
}

/* Number of set bits. */
static inline unsigned popcount64(uint64_t v)
{
    return (unsigned)__builtin_popcountll(v);
}

/* Index of the least-significant set bit; v must be non-zero. */
static inline unsigned ctz64(uint64_t v)
{
    return (unsigned)__builtin_ctzll(v);
}

/* Floor(log2(v)); v must be non-zero. */
static inline unsigned ilog2_u64(uint64_t v)
{
    return 63u - (unsigned)__builtin_clzll(v);
}

#endif /* ATAOS_KERNEL_BITS_H */
