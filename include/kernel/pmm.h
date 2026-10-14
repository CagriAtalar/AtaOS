/*
 * pmm.h - Physical Memory Manager (frame allocator).
 *
 * Implements a bitmap-tracked physical frame allocator managing 4 KiB pages
 * based on the Limine memory map (ADR-0006, docs/MEMORY_LAYOUT.md §3).
 */
#ifndef ATAOS_KERNEL_PMM_H
#define ATAOS_KERNEL_PMM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "kernel/bootinfo.h"

#define PAGE_SIZE 4096ULL
#define PMM_FRAME_SIZE PAGE_SIZE

/* Returned by pmm_alloc_frame() when physical memory is exhausted. */
#define PADDR_INVALID ((uintptr_t)-1ULL)

/* Accounting counters for physical memory tracking (docs/MEMORY_LAYOUT.md §3). */
struct pmm_stats {
    uint64_t total_frames;    /* Total frames across the managed physical address space */
    uint64_t free_frames;     /* Free frames currently available for allocation */
    uint64_t reserved_frames; /* Hardware, firmware, kernel, frame 0, and bitmap frames */
    uint64_t used_frames;     /* Frames currently allocated via pmm_alloc_frame() */
};

/*
 * Initialise the Physical Memory Manager from the Limine bootinfo structure.
 * Places the frame bitmap in the first usable region large enough, marks all
 * non-usable / reserved / kernel / frame 0 / bitmap frames as used, and clears
 * bits for usable RAM. Runs a self-test to verify alloc/free round-trip.
 */
void pmm_init(const struct bootinfo *bi);

/*
 * Allocate one 4 KiB physical frame.
 * Returns the physical address of the frame, or PADDR_INVALID on exhaustion.
 * Frame 0 (0x0..0xFFF) is never returned.
 */
uintptr_t pmm_alloc_frame(void);

/*
 * Free a previously allocated 4 KiB physical frame.
 * Panics in debug builds if paddr is unaligned, out-of-bounds, or if the frame
 * was not allocated (double-free protection).
 */
void pmm_free_frame(uintptr_t paddr);

/*
 * Allocate `count` contiguous 4 KiB physical frames.
 * Returns the physical address of the first frame, or PADDR_INVALID if a
 * contiguous block of size `count` is unavailable.
 */
uintptr_t pmm_alloc_frames(size_t count);

/*
 * Free `count` contiguous 4 KiB physical frames starting at `paddr`.
 */
void pmm_free_frames(uintptr_t paddr, size_t count);

/*
 * Query current memory accounting statistics.
 */
void pmm_get_stats(struct pmm_stats *out);

#endif /* ATAOS_KERNEL_PMM_H */
