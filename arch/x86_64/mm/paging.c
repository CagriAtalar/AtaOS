/*
 * paging.c - x86_64 page-table encoding, CR3 and CPU memory feature control.
 *
 * Owns every page-table bit encoding (M4). Prototypes come from
 * include/kernel/arch.h (the narrow interface); architecture-private bit
 * constants come from arch/paging.h. This translation unit is the only place
 * in the arch layer where page-table memory is interpreted.
 */
#include "arch/paging.h"
#include "kernel/arch.h"
#include "kernel/log.h"
#include "kernel/panic.h"

static uint64_t s_hhdm_base = 0;

void arch_mm_set_hhdm_base(uint64_t hhdm_base)
{
    s_hhdm_base = hhdm_base;
}

static inline uint64_t phys_to_virt(uint64_t paddr)
{
    return s_hhdm_base + paddr;
}

static inline uint64_t read_cr0(void)
{
    uint64_t val;
    __asm__ volatile("mov %%cr0, %0" : "=r"(val));
    return val;
}

static inline void write_cr0(uint64_t val)
{
    __asm__ volatile("mov %0, %%cr0" ::"r"(val) : "memory");
}

static inline uint64_t read_cr4(void)
{
    uint64_t val;
    __asm__ volatile("mov %%cr4, %0" : "=r"(val));
    return val;
}

static inline void write_cr4(uint64_t val)
{
    __asm__ volatile("mov %0, %%cr4" ::"r"(val) : "memory");
}

static inline uint64_t read_msr(uint32_t msr)
{
    uint32_t low, high;
    __asm__ volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32) | low;
}

static inline void
write_msr(uint32_t msr, uint32_t low, uint32_t high)
{
    __asm__ volatile("wrmsr" ::"a"(low), "d"(high), "c"(msr) : "memory");
}

static inline void cpuid(uint32_t leaf, uint32_t subleaf, uint32_t *a, uint32_t *b, uint32_t *c,
                         uint32_t *d)
{
    __asm__ volatile("cpuid"
                     : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d)
                     : "a"(leaf), "c"(subleaf));
}

void arch_mm_cpu_memory_init(void)
{
    /* CR0.WP: the supervisor honours read-only pages (mandatory). */
    uint64_t cr0 = read_cr0();
    if (!(cr0 & MM_CR0_WP)) {
        write_cr0(cr0 | MM_CR0_WP);
        log_info("vmm: CR0.WP enabled");
    }

    /* EFER.NXE: no-execute pages honoured (mandatory). */
    uint64_t efer = read_msr(IA32_EFER_MSR);
    if (!(efer & MM_EFER_NXE)) {
        write_msr(IA32_EFER_MSR, (uint32_t)(efer | MM_EFER_NXE),
                  (uint32_t)((efer | MM_EFER_NXE) >> 32));
        log_info("vmm: EFER.NXE enabled");
    }

    /*
     * CR4.SMEP / CR4.SMAP: least privilege; enabled only when CPUID leaf 7
     * reports them (SMEP = EBX bit 7, SMAP = EBX bit 20). qemu64 reports
     * neither, so absence is logged, not fatal.
     */
    uint32_t a = 0, b = 0, c = 0, d = 0;
    cpuid(7, 0, &a, &b, &c, &d);
    bool has_smep = (b & MM_CPUID7_EBX_SMEP) != 0;
    bool has_smap = (b & MM_CPUID7_EBX_SMAP) != 0;

    uint64_t cr4 = read_cr4();
    uint64_t new_cr4 = cr4;
    if (has_smep && !(cr4 & MM_CR4_SMEP))
        new_cr4 |= MM_CR4_SMEP;
    if (has_smap && !(cr4 & MM_CR4_SMAP))
        new_cr4 |= MM_CR4_SMAP;
    if (new_cr4 != cr4)
        write_cr4(new_cr4);

    log_info("vmm: cpu memory features (SMEP %s, SMAP %s)", has_smep ? "on" : "unavailable",
             has_smap ? "on" : "unavailable");
}

uint64_t arch_mm_table_read(uint64_t table_phys, size_t index)
{
    const volatile uint64_t *table =
        (const volatile uint64_t *)(uintptr_t)phys_to_virt(table_phys);
    return table[index];
}

void arch_mm_table_write(uint64_t table_phys, size_t index, uint64_t entry)
{
    volatile uint64_t *table = (volatile uint64_t *)(uintptr_t)phys_to_virt(table_phys);
    table[index] = entry;
}

void arch_mm_table_zero(uint64_t table_phys)
{
    volatile uint64_t *table = (volatile uint64_t *)(uintptr_t)phys_to_virt(table_phys);
    for (size_t i = 0; i < MM_TABLE_ENTRIES; i++)
        table[i] = 0;
}

uint64_t arch_mm_read_cr3(void)
{
    uint64_t val;
    __asm__ volatile("mov %%cr3, %0" : "=r"(val));
    return val;
}

void arch_mm_set_cr3(uint64_t pml4_phys)
{
    __asm__ volatile("mov %0, %%cr3" ::"r"(pml4_phys & MM_ADDR_MASK) : "memory");
}

void arch_mm_invlpg(uint64_t vaddr)
{
    __asm__ volatile("invlpg (%0)" ::"r"(vaddr) : "memory");
}
