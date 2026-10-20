/*
 * fake_pmm.c - deterministic physical-frame allocator for host tests.
 *
 * Frames are addresses in the fake_arch arena. The allocator intentionally
 * mirrors the PMM contract relevant to kernel/mm: frame 0 is never returned,
 * exhaustion reports PADDR_INVALID, and stats track every allocation.
 */
#include "fake_pmm.h"
#include "fake_arch.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <kernel/pmm.h>

static bool s_used[FAKE_FRAME_COUNT];
static struct pmm_stats s_stats;
static size_t s_fail_after = SIZE_MAX;
static size_t s_successes;

static void fake_pmm_recount(void)
{
    uint64_t used = 0;
    for (size_t i = 0; i < FAKE_FRAME_COUNT; i++) {
        if (s_used[i])
            used++;
    }
    s_stats.total_frames = FAKE_FRAME_COUNT;
    s_stats.used_frames = used;
    s_stats.free_frames = FAKE_FRAME_COUNT - used;
    s_stats.reserved_frames = 1; /* frame zero */
}

void fake_pmm_reset(void)
{
    memset(s_used, 0, sizeof(s_used));
    s_used[0] = true;
    s_fail_after = SIZE_MAX;
    s_successes = 0;
    fake_pmm_recount();
}

void fake_pmm_fail_after(size_t successes)
{
    s_fail_after = successes;
}

size_t fake_pmm_allocated(void)
{
    return (size_t)s_stats.used_frames;
}

static bool can_allocate(void)
{
    return s_successes < s_fail_after;
}

uintptr_t pmm_alloc_frame(void)
{
    if (!can_allocate())
        return PADDR_INVALID;

    for (size_t i = 1; i < FAKE_FRAME_COUNT; i++) {
        if (s_used[i])
            continue;
        s_used[i] = true;
        s_successes++;
        fake_pmm_recount();
        return (uintptr_t)(FAKE_PHYS_BASE + (uint64_t)i * FAKE_FRAME_SIZE);
    }
    return PADDR_INVALID;
}

uintptr_t pmm_alloc_frames(size_t count)
{
    if (count == 0)
        return PADDR_INVALID;
    if (count == 1)
        return pmm_alloc_frame();
    if (!can_allocate())
        return PADDR_INVALID;
    if (s_stats.free_frames < count)
        return PADDR_INVALID;

    for (size_t start = 1; start + count <= FAKE_FRAME_COUNT; start++) {
        size_t i = 0;
        while (i < count && !s_used[start + i])
            i++;
        if (i != count)
            continue;

        for (i = 0; i < count; i++)
            s_used[start + i] = true;
        s_successes++;
        fake_pmm_recount();
        return (uintptr_t)(FAKE_PHYS_BASE + (uint64_t)start * FAKE_FRAME_SIZE);
    }
    return PADDR_INVALID;
}

void pmm_free_frame(uintptr_t paddr)
{
    if (paddr < FAKE_PHYS_BASE || (paddr % FAKE_FRAME_SIZE) != 0)
        abort();

    size_t frame = (size_t)((paddr - FAKE_PHYS_BASE) / FAKE_FRAME_SIZE);
    if (frame == 0 || frame >= FAKE_FRAME_COUNT || !s_used[frame])
        abort();

    s_used[frame] = false;
    fake_pmm_recount();
}

void pmm_free_frames(uintptr_t paddr, size_t count)
{
    for (size_t i = 0; i < count; i++)
        pmm_free_frame(paddr + i * FAKE_FRAME_SIZE);
}

void pmm_get_stats(struct pmm_stats *out)
{
    if (out)
        *out = s_stats;
}

void pmm_init(const struct bootinfo *bi)
{
    (void)bi;
    fake_pmm_reset();
}

uintptr_t pmm_bitmap_phys(void)
{
    return (uintptr_t)FAKE_PHYS_BASE;
}
