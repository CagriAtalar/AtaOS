/*
 * heap.h - Kernel dynamic memory allocator (kmalloc, kcalloc, kfree).
 *
 * Implements a header-based first-fit allocator with split/coalesce
 * over physical memory frames accessed via HHDM (docs/MEMORY_LAYOUT.md §8).
 */
#ifndef ATAOS_KERNEL_HEAP_H
#define ATAOS_KERNEL_HEAP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define HEAP_ALIGNMENT 16ULL

struct heap_stats {
    size_t total_bytes;      /* Total capacity across all heap arenas */
    size_t allocated_bytes;  /* Bytes currently allocated to callers */
    size_t free_bytes;       /* Bytes available in free blocks */
    size_t allocation_count; /* Number of currently active allocations */
};

/*
 * Initialise the kernel heap.
 * Allocates initial backing frames from the PMM through HHDM.
 */
void heap_init(void);

/*
 * Initialise a heap arena over a caller-provided memory buffer.
 * Used for host testing and early bootstrap testing.
 */
bool heap_add_arena(void *buffer, size_t size);

/*
 * Allocate `size` bytes of memory aligned to at least 16 bytes.
 * Returns NULL on failure or if size == 0.
 */
void *kmalloc(size_t size);

/*
 * Allocate `n * size` bytes initialized to zero.
 * Rejects integer multiplication overflow by returning NULL.
 * Returns NULL if n == 0 or size == 0.
 */
void *kcalloc(size_t n, size_t size);

/*
 * Free memory previously allocated by kmalloc or kcalloc.
 * kfree(NULL) is a safe no-op.
 * Detects double-free and invalid pointer corruption in debug builds.
 */
void kfree(void *ptr);

/*
 * Query current heap accounting statistics.
 */
void heap_get_stats(struct heap_stats *out);

#endif /* ATAOS_KERNEL_HEAP_H */
