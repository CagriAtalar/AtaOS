/*
 * pmm.c - Physical Memory Manager (frame allocator).
 *
 * Implements a bitmap-tracked physical frame allocator managing 4 KiB pages
 * based on the Limine memory map (ADR-0006, docs/MEMORY_LAYOUT.md §3).
 */
#include "kernel/pmm.h"
#include "kernel/bitmap.h"
#include "kernel/bits.h"
#include "kernel/bootinfo.h"
#include "kernel/log.h"
#include "kernel/panic.h"
#include "kernel/string.h"

static struct bitmap s_bitmap;
static struct pmm_stats s_stats;
static size_t s_next_fit_hint = 0;
static uint64_t s_hhdm_offset = 0;
static uintptr_t s_bitmap_paddr = 0;
static bool s_pmm_initialised = false;

static void pmm_self_test(void)
{
    struct pmm_stats before;
    pmm_get_stats(&before);

    /* Test single frame allocation */
    uintptr_t f1 = pmm_alloc_frame();
    ASSERT_ALWAYS(f1 != PADDR_INVALID);
    ASSERT_ALWAYS(f1 != 0);
    ASSERT_ALWAYS(is_aligned(f1, PAGE_SIZE));

    /* Test read/write via HHDM */
    volatile uint64_t *ptr = (volatile uint64_t *)(s_hhdm_offset + f1);
    *ptr = 0xAA55AA55DEADBEEFULL;
    ASSERT_ALWAYS(*ptr == 0xAA55AA55DEADBEEFULL);

    ASSERT_ALWAYS(s_stats.used_frames == 1);
    ASSERT_ALWAYS(s_stats.free_frames == before.free_frames - 1);

    /* Test free */
    pmm_free_frame(f1);
    ASSERT_ALWAYS(s_stats.used_frames == 0);
    ASSERT_ALWAYS(s_stats.free_frames == before.free_frames);

    /* Test multi-frame contiguous allocation */
    uintptr_t f4 = pmm_alloc_frames(4);
    ASSERT_ALWAYS(f4 != PADDR_INVALID);
    ASSERT_ALWAYS(f4 != 0);
    ASSERT_ALWAYS(is_aligned(f4, PAGE_SIZE));

    ASSERT_ALWAYS(s_stats.used_frames == 4);
    ASSERT_ALWAYS(s_stats.free_frames == before.free_frames - 4);

    pmm_free_frames(f4, 4);
    ASSERT_ALWAYS(s_stats.used_frames == 0);
    ASSERT_ALWAYS(s_stats.free_frames == before.free_frames);

    log_info("pmm: self-test passed (alloc/free round-trip verified)");
}

void pmm_init(const struct bootinfo *bi)
{
    ASSERT_ALWAYS(bi != NULL);
    ASSERT_ALWAYS(bi->memmap_count > 0);
    ASSERT_ALWAYS(bi->hhdm_offset != 0);

    /*
     * Scan memory map to find the highest physical address among RAM regions.
     * High firmware-reserved MMIO ranges (e.g. PCI MMIO at 1 TiB) are not RAM
     * and are never managed or returned by the PMM (docs/MEMORY_LAYOUT.md §3).
     */
    uint64_t highest_phys_addr = 0;
    for (size_t i = 0; i < bi->memmap_count; i++) {
        const struct bootinfo_mem_region *r = &bi->memmap[i];
        if (r->type == BOOT_MEM_USABLE || r->type == BOOT_MEM_BOOTLOADER_RECLAIMABLE ||
            r->type == BOOT_MEM_KERNEL_AND_MODULES || r->type == BOOT_MEM_ACPI_RECLAIMABLE) {
            uint64_t end;
            if (add_overflows_u64(r->base, r->length, &end)) {
                PANIC("pmm: memory region overflow at base 0x%lx len 0x%lx", (unsigned long)r->base,
                      (unsigned long)r->length);
            }
            if (end > highest_phys_addr)
                highest_phys_addr = end;
        }
    }

    uint64_t total_frames = align_up(highest_phys_addr, PAGE_SIZE) / PAGE_SIZE;
    if (total_frames == 0) {
        PANIC("pmm: no physical memory found in memory map");
    }

    size_t bitmap_bytes = bitmap_byte_size(total_frames);
    size_t bitmap_frames = (bitmap_bytes + PAGE_SIZE - 1) / PAGE_SIZE;
    size_t bitmap_alloc_size = bitmap_frames * PAGE_SIZE;

    /* Find the first usable region large enough to hold the bitmap (ADR-0006) */
    uint64_t bitmap_paddr = 0;
    bool found_region = false;

    for (size_t i = 0; i < bi->memmap_count; i++) {
        const struct bootinfo_mem_region *r = &bi->memmap[i];
        if (r->type != BOOT_MEM_USABLE)
            continue;

        uint64_t reg_start = align_up(r->base, PAGE_SIZE);
        uint64_t reg_end = align_down(r->base + r->length, PAGE_SIZE);

        /* Never place bitmap in frame 0 */
        if (reg_start < PAGE_SIZE)
            reg_start = PAGE_SIZE;

        if (reg_end > reg_start && (reg_end - reg_start) >= bitmap_alloc_size) {
            bitmap_paddr = reg_start;
            found_region = true;
            break;
        }
    }

    if (!found_region) {
        PANIC("pmm: failed to find usable memory for bitmap (%zu bytes required)",
              bitmap_alloc_size);
    }

    /* Initialize bitmap through HHDM with all bits set to 1 (used/reserved) */
    void *bitmap_vaddr = (void *)(bi->hhdm_offset + bitmap_paddr);
    bitmap_init(&s_bitmap, bitmap_vaddr, total_frames, true);

    /* Mark all USABLE regions within managed RAM as free (0) */
    for (size_t i = 0; i < bi->memmap_count; i++) {
        const struct bootinfo_mem_region *r = &bi->memmap[i];
        if (r->type == BOOT_MEM_USABLE) {
            uint64_t start = align_up(r->base, PAGE_SIZE);
            uint64_t end = align_down(r->base + r->length, PAGE_SIZE);
            if (end > highest_phys_addr)
                end = highest_phys_addr;
            if (end > start) {
                size_t sf = start / PAGE_SIZE;
                size_t count = (end - start) / PAGE_SIZE;
                bitmap_clear_range(&s_bitmap, sf, count);
            }
        }
    }

    /* Re-reserve critical regions:
     * 1. Frame 0 (0x0..0xFFF) is never allocated (null-pointer hardening).
     * 2. The bitmap metadata itself.
     * 3. Non-usable regions within managed range (reserved, ACPI, bad, framebuffer, bootloader reclaimable).
     */
    bitmap_set(&s_bitmap, 0);

    size_t bm_start_frame = bitmap_paddr / PAGE_SIZE;
    bitmap_set_range(&s_bitmap, bm_start_frame, bitmap_frames);

    for (size_t i = 0; i < bi->memmap_count; i++) {
        const struct bootinfo_mem_region *r = &bi->memmap[i];
        if (r->type != BOOT_MEM_USABLE) {
            if (r->base >= highest_phys_addr)
                continue;
            uint64_t start = align_down(r->base, PAGE_SIZE);
            uint64_t end = align_up(r->base + r->length, PAGE_SIZE);
            if (end > highest_phys_addr)
                end = highest_phys_addr;
            if (end > start) {
                size_t sf = start / PAGE_SIZE;
                size_t count = (end - start) / PAGE_SIZE;
                bitmap_set_range(&s_bitmap, sf, count);
            }
        }
    }

    if (bi->kernel_addr_valid && bi->kernel_phys_base < highest_phys_addr) {
        size_t kf = bi->kernel_phys_base / PAGE_SIZE;
        bitmap_set(&s_bitmap, kf);
    }

    /* Calculate free frames count using fast word-level popcount */
    uint64_t free_count = 0;
    size_t num_words = (total_frames + 63) / 64;
    for (size_t w = 0; w < num_words; w++) {
        uint64_t free_bits = ~s_bitmap.words[w];
        if (w == num_words - 1 && (total_frames % 64 != 0)) {
            uint64_t mask = (1ULL << (total_frames % 64)) - 1;
            free_bits &= mask;
        }
        free_count += popcount64(free_bits);
    }

    s_stats.total_frames = total_frames;
    s_stats.free_frames = free_count;
    s_stats.reserved_frames = total_frames - free_count;
    s_stats.used_frames = 0;
    s_next_fit_hint = 0;
    s_hhdm_offset = bi->hhdm_offset;
    s_bitmap_paddr = (uintptr_t)bitmap_paddr;
    s_pmm_initialised = true;

    log_info("pmm: %lu MiB managed (%lu frames), %lu KiB free (%lu frames), %lu KiB reserved (%lu "
             "frames)",
             (unsigned long)((total_frames * PAGE_SIZE) / (1024 * 1024)),
             (unsigned long)total_frames, (unsigned long)((free_count * PAGE_SIZE) / 1024),
             (unsigned long)free_count, (unsigned long)((s_stats.reserved_frames * PAGE_SIZE) / 1024),
             (unsigned long)s_stats.reserved_frames);

    pmm_self_test();
}

uintptr_t pmm_alloc_frame(void)
{
    ASSERT_ALWAYS(s_pmm_initialised);

    if (s_stats.free_frames == 0)
        return PADDR_INVALID;

    size_t frame = bitmap_find_first_free(&s_bitmap, &s_next_fit_hint);
    if (frame == SIZE_MAX)
        return PADDR_INVALID;

    ASSERT(frame != 0);

    bitmap_set(&s_bitmap, frame);
    s_stats.free_frames--;
    s_stats.used_frames++;

    return (uintptr_t)(frame * PAGE_SIZE);
}

void pmm_free_frame(uintptr_t paddr)
{
    ASSERT_ALWAYS(s_pmm_initialised);
    ASSERT(is_aligned(paddr, PAGE_SIZE));
    ASSERT(paddr != 0);

    size_t frame = paddr / PAGE_SIZE;
    ASSERT(frame < s_stats.total_frames);

    if (!bitmap_test(&s_bitmap, frame)) {
        PANIC("pmm: double-free detected for physical frame 0x%lx", (unsigned long)paddr);
    }

    bitmap_clear(&s_bitmap, frame);
    s_stats.free_frames++;
    s_stats.used_frames--;
}

uintptr_t pmm_alloc_frames(size_t count)
{
    ASSERT_ALWAYS(s_pmm_initialised);

    if (count == 0)
        return PADDR_INVALID;

    if (count == 1)
        return pmm_alloc_frame();

    if (s_stats.free_frames < count)
        return PADDR_INVALID;

    size_t frame = bitmap_find_contiguous_free(&s_bitmap, count, &s_next_fit_hint);
    if (frame == SIZE_MAX)
        return PADDR_INVALID;

    ASSERT(frame != 0);

    bitmap_set_range(&s_bitmap, frame, count);
    s_stats.free_frames -= count;
    s_stats.used_frames += count;

    return (uintptr_t)(frame * PAGE_SIZE);
}

void pmm_free_frames(uintptr_t paddr, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        pmm_free_frame(paddr + i * PAGE_SIZE);
    }
}

void pmm_get_stats(struct pmm_stats *out)
{
    if (out)
        *out = s_stats;
}

uintptr_t pmm_bitmap_phys(void)
{
    return s_bitmap_paddr;
}
