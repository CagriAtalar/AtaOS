/*
 * fake_arch_mm.c - host-test page-table memory and control-register hooks.
 *
 * The arena is zero-filled and indexed by physical address. All page-table
 * frames allocated by fake_pmm.c point into it, so the production walkers in
 * kernel/mm can operate unchanged.
 */
#include "fake_arch.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <kernel/arch.h>
#include <kernel/mm_types.h>

static uint8_t s_arena[FAKE_FRAME_COUNT * FAKE_FRAME_SIZE]
    __attribute__((aligned(FAKE_FRAME_SIZE)));

static uint64_t s_cr3;
static uint64_t s_hhdm_base;

uint8_t *fake_phys_ptr(uint64_t paddr)
{
    if (paddr < FAKE_PHYS_BASE)
        return NULL;
    uint64_t offset = paddr - FAKE_PHYS_BASE;
    if (offset >= sizeof(s_arena))
        return NULL;
    return &s_arena[offset];
}

void fake_arch_reset(void)
{
    memset(s_arena, 0, sizeof(s_arena));
    s_cr3 = 0;
    s_hhdm_base = 0;
}

void arch_mm_set_hhdm_base(uint64_t hhdm_base)
{
    s_hhdm_base = hhdm_base;
}

void arch_mm_cpu_memory_init(void)
{
}

uint64_t arch_mm_table_read(uint64_t table_phys, size_t index)
{
    uint64_t *table = (uint64_t *)fake_phys_ptr(table_phys);
    if (!table || index >= ARCH_MM4_TABLE_ENTRIES) {
        fprintf(stderr, "fake_arch: bad table read phys=0x%lx index=%zu\n",
                (unsigned long)table_phys, index);
        abort();
    }
    return table[index];
}

void arch_mm_table_write(uint64_t table_phys, size_t index, uint64_t entry)
{
    uint64_t *table = (uint64_t *)fake_phys_ptr(table_phys);
    if (!table || index >= ARCH_MM4_TABLE_ENTRIES) {
        fprintf(stderr, "fake_arch: bad table write phys=0x%lx index=%zu\n",
                (unsigned long)table_phys, index);
        abort();
    }
    table[index] = entry;
}

void arch_mm_table_zero(uint64_t table_phys)
{
    void *table = fake_phys_ptr(table_phys);
    if (!table) {
        fprintf(stderr, "fake_arch: bad table zero phys=0x%lx\n",
                (unsigned long)table_phys);
        abort();
    }
    memset(table, 0, PAGE_SIZE);
}

uint64_t arch_mm_read_cr3(void)
{
    return s_cr3;
}

void arch_mm_set_cr3(uint64_t pml4_phys)
{
    s_cr3 = pml4_phys;
}

void arch_mm_invlpg(uint64_t vaddr)
{
    (void)vaddr;
}
