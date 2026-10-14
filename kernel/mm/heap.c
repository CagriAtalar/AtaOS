/*
 * heap.c - Kernel dynamic memory allocator (kmalloc, kcalloc, kfree).
 *
 * Implements a header-based first-fit allocator with split/coalesce
 * over physical memory frames accessed via HHDM (docs/MEMORY_LAYOUT.md §8).
 * Pure and host-testable under ASan/UBSan.
 */
#include "kernel/heap.h"
#include "kernel/bits.h"
#include "kernel/bootinfo.h"
#include "kernel/log.h"
#include "kernel/panic.h"
#include "kernel/pmm.h"
#include "kernel/string.h"

#define HEAP_MAGIC_ALLOC 0x4154414845415031ULL /* "ATAHEAP1" */
#define HEAP_MAGIC_FREE  0x4154414845415030ULL /* "ATAHEAP0" */
#define HEAP_INITIAL_FRAMES 16                 /* 64 KiB initial pool */
#define HEAP_GROW_FRAMES    16                 /* 64 KiB growth step */

struct heap_block {
    uint64_t magic;
    size_t size;             /* Usable payload size in bytes */
    struct heap_block *next; /* Next block in the same arena */
    struct heap_block *prev; /* Previous block in the same arena */
};

struct heap_arena {
    struct heap_arena *next_arena;
    size_t total_size;
    struct heap_block *first_block;
};

static struct heap_arena *s_first_arena = NULL;
static struct heap_stats s_stats = {0, 0, 0, 0};
#ifndef ATAOS_HOST_TEST
static bool s_heap_initialised = false;
#endif

bool heap_add_arena(void *buffer, size_t size)
{
    if (!buffer || size == 0)
        return false;

    uintptr_t start = align_up((uintptr_t)buffer, HEAP_ALIGNMENT);
    uintptr_t end = align_down((uintptr_t)buffer + size, HEAP_ALIGNMENT);

    size_t min_needed = sizeof(struct heap_arena) + sizeof(struct heap_block) + HEAP_ALIGNMENT;
    if (end <= start || (end - start) < min_needed)
        return false;

    struct heap_arena *arena = (struct heap_arena *)(void *)start;
    uintptr_t block_addr = align_up(start + sizeof(struct heap_arena), HEAP_ALIGNMENT);
    struct heap_block *first = (struct heap_block *)(void *)block_addr;

    size_t available_payload = end - (block_addr + sizeof(struct heap_block));

    first->magic = HEAP_MAGIC_FREE;
    first->size = available_payload;
    first->next = NULL;
    first->prev = NULL;

    arena->next_arena = s_first_arena;
    arena->total_size = end - start;
    arena->first_block = first;

    s_first_arena = arena;
    s_stats.total_bytes += available_payload;
    s_stats.free_bytes += available_payload;

    return true;
}

static bool heap_grow(size_t needed_payload)
{
#ifndef ATAOS_HOST_TEST
    size_t min_bytes = needed_payload + sizeof(struct heap_arena) + sizeof(struct heap_block) +
                       HEAP_ALIGNMENT;
    size_t frames_needed = (min_bytes + PAGE_SIZE - 1) / PAGE_SIZE;
    if (frames_needed < HEAP_GROW_FRAMES)
        frames_needed = HEAP_GROW_FRAMES;

    uintptr_t paddr = pmm_alloc_frames(frames_needed);
    if (paddr == PADDR_INVALID)
        return false;

    const struct bootinfo *bi = bootinfo_get();
    if (!bi || bi->hhdm_offset == 0)
        return false;

    void *vaddr = (void *)(bi->hhdm_offset + paddr);
    return heap_add_arena(vaddr, frames_needed * PAGE_SIZE);
#else
    (void)needed_payload;
    return false;
#endif
}

void *kmalloc(size_t size)
{
    if (size == 0)
        return NULL;

    size_t needed = (size_t)align_up(size, HEAP_ALIGNMENT);

    for (int retry = 0; retry < 2; retry++) {
        for (struct heap_arena *arena = s_first_arena; arena != NULL; arena = arena->next_arena) {
            for (struct heap_block *b = arena->first_block; b != NULL; b = b->next) {
                if (b->magic == HEAP_MAGIC_FREE && b->size >= needed) {
                    /* Check if we can split this block */
                    if (b->size >= needed + sizeof(struct heap_block) + HEAP_ALIGNMENT) {
                        struct heap_block *split =
                            (struct heap_block *)(void *)((uint8_t *)(b + 1) + needed);
                        split->magic = HEAP_MAGIC_FREE;
                        split->size = b->size - needed - sizeof(struct heap_block);
                        split->next = b->next;
                        split->prev = b;
                        if (b->next)
                            b->next->prev = split;
                        b->next = split;
                        b->size = needed;

                        s_stats.total_bytes -= sizeof(struct heap_block);
                        s_stats.free_bytes -= sizeof(struct heap_block);
                    }

                    b->magic = HEAP_MAGIC_ALLOC;
                    s_stats.allocated_bytes += b->size;
                    s_stats.free_bytes -= b->size;
                    s_stats.allocation_count++;

                    return (void *)(b + 1);
                }
            }
        }

        /* Not enough free space; attempt to grow the heap with more frames */
        if (!heap_grow(needed))
            break;
    }

    return NULL;
}

void *kcalloc(size_t n, size_t size)
{
    uint64_t total;
    if (mul_overflows_u64((uint64_t)n, (uint64_t)size, &total))
        return NULL;

    if (total == 0)
        return NULL;

    void *ptr = kmalloc((size_t)total);
    if (ptr)
        memset(ptr, 0, (size_t)total);

    return ptr;
}

void kfree(void *ptr)
{
    if (!ptr)
        return;

    struct heap_block *b = (struct heap_block *)ptr - 1;

    if (b->magic == HEAP_MAGIC_FREE) {
        PANIC("heap: double-free detected for pointer %p", ptr);
    }
    if (b->magic != HEAP_MAGIC_ALLOC) {
        PANIC("heap: corrupted header or invalid pointer %p (magic 0x%lx)", ptr,
              (unsigned long)b->magic);
    }

    b->magic = HEAP_MAGIC_FREE;
    s_stats.allocated_bytes -= b->size;
    s_stats.free_bytes += b->size;
    s_stats.allocation_count--;

#ifdef ATAOS_DEBUG
    /* Memory poisoning to detect use-after-free bugs (ADR-0016) */
    memset(ptr, 0x55, b->size);
#endif

    /* Coalesce with adjacent next block if free */
    if (b->next != NULL && b->next->magic == HEAP_MAGIC_FREE) {
        if ((uint8_t *)(b + 1) + b->size == (uint8_t *)b->next) {
            struct heap_block *next = b->next;
            b->size += sizeof(struct heap_block) + next->size;
            b->next = next->next;
            if (b->next)
                b->next->prev = b;
            s_stats.total_bytes += sizeof(struct heap_block);
            s_stats.free_bytes += sizeof(struct heap_block);
        }
    }

    /* Coalesce with adjacent previous block if free */
    if (b->prev != NULL && b->prev->magic == HEAP_MAGIC_FREE) {
        if ((uint8_t *)(b->prev + 1) + b->prev->size == (uint8_t *)b) {
            struct heap_block *prev = b->prev;
            prev->size += sizeof(struct heap_block) + b->size;
            prev->next = b->next;
            if (b->next)
                b->next->prev = prev;
            s_stats.total_bytes += sizeof(struct heap_block);
            s_stats.free_bytes += sizeof(struct heap_block);
        }
    }
}

void heap_get_stats(struct heap_stats *out)
{
    if (out)
        *out = s_stats;
}

#ifndef ATAOS_HOST_TEST
static void heap_self_test(void)
{
    struct heap_stats before;
    heap_get_stats(&before);

    /* 1. Basic allocation and 16-byte alignment */
    void *p1 = kmalloc(48);
    ASSERT_ALWAYS(p1 != NULL);
    ASSERT_ALWAYS(is_aligned((uintptr_t)p1, HEAP_ALIGNMENT));
    memset(p1, 0xAB, 48);

    /* 2. kcalloc zero-initialization */
    uint32_t *arr = (uint32_t *)kcalloc(8, sizeof(uint32_t));
    ASSERT_ALWAYS(arr != NULL);
    ASSERT_ALWAYS(is_aligned((uintptr_t)arr, HEAP_ALIGNMENT));
    for (int i = 0; i < 8; i++) {
        ASSERT_ALWAYS(arr[i] == 0);
    }

    /* 3. Integer overflow rejection in kcalloc */
    void *bad_overflow = kcalloc(UINT64_MAX / 2, 4);
    ASSERT_ALWAYS(bad_overflow == NULL);

    /* 4. Zero size rejection */
    ASSERT_ALWAYS(kmalloc(0) == NULL);
    ASSERT_ALWAYS(kcalloc(0, 10) == NULL);

    /* 5. kfree(NULL) safe no-op */
    kfree(NULL);

    /* 6. Release memory and verify coalescing */
    kfree(arr);
    kfree(p1);

    struct heap_stats after;
    heap_get_stats(&after);

    ASSERT_ALWAYS(after.allocation_count == before.allocation_count);
    ASSERT_ALWAYS(after.allocated_bytes == before.allocated_bytes);
    ASSERT_ALWAYS(after.free_bytes == before.free_bytes);

    log_info("heap: self-test passed (split/coalesce and overflow checked)");
}

void heap_init(void)
{
    uintptr_t paddr = pmm_alloc_frames(HEAP_INITIAL_FRAMES);
    if (paddr == PADDR_INVALID) {
        PANIC("heap: failed to allocate initial physical frames");
    }

    const struct bootinfo *bi = bootinfo_get();
    ASSERT_ALWAYS(bi != NULL);
    ASSERT_ALWAYS(bi->hhdm_offset != 0);

    void *vaddr = (void *)(bi->hhdm_offset + paddr);
    bool ok = heap_add_arena(vaddr, HEAP_INITIAL_FRAMES * PAGE_SIZE);
    if (!ok) {
        PANIC("heap: failed to initialize initial heap arena");
    }

    s_heap_initialised = true;

    log_info("heap: %lu KiB managed, %lu KiB free", (unsigned long)(s_stats.total_bytes / 1024),
             (unsigned long)(s_stats.free_bytes / 1024));

    heap_self_test();
}
#endif
