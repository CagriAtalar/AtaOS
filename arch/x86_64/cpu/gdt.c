/*
 * gdt.c - x86_64 Global Descriptor Table (GDT) and Task State Segment (TSS).
 *
 * Implements M2 Task 1:
 *   - 64-bit kernel code/data descriptors
 *   - 64-bit user code/data descriptors (SYSRET layout compliant)
 *   - 64-bit Task State Segment (TSS) with bootstrap RSP0 and dedicated IST stacks
 *   - Writable GDT to allow hardware TSS busy bit updates on `ltr`
 */
#include "arch/gdt.h"
#include "kernel/arch.h"
#include "kernel/compiler.h"
#include "kernel/log.h"
#include "kernel/string.h"

/* The boot stack top defined in arch/x86_64/boot/entry.S */
extern char boot_stack_top[];

/*
 * Static, 16-byte aligned IST stacks for critical CPU exceptions (ADR-0008):
 *   IST1: Double Fault (#DF) - prevents triple fault on kernel stack exhaustion
 *   IST2: Non-Maskable Interrupt (NMI)
 *   IST3: Machine Check (#MC)
 */
static uint8_t s_df_stack[GDT_IST_STACK_SIZE] ATAOS_ALIGNED(16);
static uint8_t s_nmi_stack[GDT_IST_STACK_SIZE] ATAOS_ALIGNED(16);
static uint8_t s_mc_stack[GDT_IST_STACK_SIZE] ATAOS_ALIGNED(16);

/*
 * Active Task State Segment (TSS).
 * Packed 104-byte structure as specified by AMD64 Architecture Manual.
 */
static struct tss64 s_tss ATAOS_ALIGNED(16);

/*
 * Global Descriptor Table.
 * Must reside in writable memory (.data) because `ltr` updates the TSS
 * descriptor's Type field from 0x9 (Available) to 0xB (Busy).
 *
 * Entries:
 *   [0] 0x00: Null descriptor
 *   [1] 0x08: Kernel 64-bit code
 *   [2] 0x10: Kernel 64-bit data
 *   [3] 0x18: User 64-bit data (RPL3 = 0x1B)
 *   [4] 0x20: User 64-bit code (RPL3 = 0x23)
 *   [5] 0x28: TSS descriptor (low 64 bits)
 *   [6] 0x30: TSS descriptor (high 64 bits)
 */
#define GDT_ENTRY_COUNT 7
static uint64_t s_gdt[GDT_ENTRY_COUNT] ATAOS_ALIGNED(16);

static struct gdtr s_gdtr;

static void set_tss_descriptor(uint64_t *entry_low, uint64_t *entry_high,
                              uintptr_t base, size_t limit)
{
    /*
     * 64-bit System Segment Descriptor (TSS) format:
     * Low 64 bits:
     *   [15:0]  Limit 15:0
     *   [39:16] Base 23:0
     *   [43:40] Type = 1001b (0x9, 64-bit TSS available)
     *   [44]    S = 0 (system descriptor)
     *   [46:45] DPL = 00b (Ring 0)
     *   [47]    P = 1 (Present)
     *   [51:48] Limit 19:16
     *   [52]    AVL = 0
     *   [54:53] Reserved = 0
     *   [55]    G = 0 (byte granularity)
     *   [63:56] Base 31:24
     * High 64 bits:
     *   [31:0]  Base 63:32
     *   [63:32] Reserved (must be zero)
     */
    uint64_t low = (uint64_t)(limit & 0xFFFFULL);
    low |= ((uint64_t)base & 0x00FFFFFFULL) << 16;
    low |= 0x89ULL << 40; /* P=1, DPL=0, S=0, Type=9 */
    low |= ((uint64_t)(limit >> 16) & 0x0FULL) << 48;
    low |= (((uint64_t)base >> 24) & 0xFFULL) << 56;

    uint64_t high = ((uint64_t)base >> 32) & 0xFFFFFFFFULL;

    *entry_low = low;
    *entry_high = high;
}

void arch_gdt_init(void)
{
    memset(&s_tss, 0, sizeof(s_tss));

    /*
     * Initialize bootstrap kernel stack pointer (RSP0).
     * This ensures the CPU has a valid kernel stack if an interrupt or
     * transition from unprivileged mode occurs before the scheduler exists.
     * NOTE: In M6, the scheduler MUST update s_tss.rsp0 to the current thread's
     * kernel stack during every context switch.
     */
    s_tss.rsp0 = (uint64_t)(uintptr_t)boot_stack_top;

    /*
     * Populate dedicated IST stack tops (stacks grow downward).
     * Tops must be 16-byte aligned.
     */
    s_tss.ist1 = (uint64_t)(uintptr_t)(s_df_stack + GDT_IST_STACK_SIZE);
    s_tss.ist2 = (uint64_t)(uintptr_t)(s_nmi_stack + GDT_IST_STACK_SIZE);
    s_tss.ist3 = (uint64_t)(uintptr_t)(s_mc_stack + GDT_IST_STACK_SIZE);

    /*
     * Set IOPB offset to the size of TSS, meaning the I/O permission bitmap
     * is empty and all port I/O from Ring 3 will fault unless permitted.
     */
    s_tss.iopb_offset = (uint16_t)sizeof(s_tss);

    /* 0x00: Null descriptor */
    s_gdt[0] = 0x0000000000000000ULL;

    /*
     * 0x08: Kernel 64-bit Code
     * Limit: 0xFFFF, Base: 0, Access: 0x9A (P=1, DPL=0, S=1, Exec/Read),
     * Flags: 0xAF (G=1, L=1, D=0, Limit_high=0xF)
     */
    s_gdt[1] = 0x00AF9A000000FFFFULL;

    /*
     * 0x10: Kernel 64-bit Data
     * Limit: 0xFFFF, Base: 0, Access: 0x92 (P=1, DPL=0, S=1, Read/Write),
     * Flags: 0xCF (G=1, D/B=1, Limit_high=0xF)
     */
    s_gdt[2] = 0x00CF92000000FFFFULL;

    /*
     * 0x18: User 64-bit Data (RPL 3 = 0x1B)
     * Limit: 0xFFFF, Base: 0, Access: 0xF2 (P=1, DPL=3, S=1, Read/Write),
     * Flags: 0xCF (G=1, D/B=1, Limit_high=0xF)
     */
    s_gdt[3] = 0x00CFF2000000FFFFULL;

    /*
     * 0x20: User 64-bit Code (RPL 3 = 0x23)
     * Limit: 0xFFFF, Base: 0, Access: 0xFA (P=1, DPL=3, S=1, Exec/Read),
     * Flags: 0xAF (G=1, L=1, D=0, Limit_high=0xF)
     */
    s_gdt[4] = 0x00AFFA000000FFFFULL;

    /*
     * 0x28 & 0x30: 64-bit TSS Descriptor (16 bytes)
     */
    set_tss_descriptor(&s_gdt[5], &s_gdt[6], (uintptr_t)&s_tss, sizeof(s_tss) - 1);

    /* Configure GDTR */
    s_gdtr.limit = (uint16_t)(sizeof(s_gdt) - 1);
    s_gdtr.base = (uint64_t)(uintptr_t)&s_gdt[0];

    /* Load GDTR, reload CS/DS/ES/SS, and load Task Register */
    arch_gdt_flush(&s_gdtr, GDT_KERNEL_CODE_SEL, GDT_KERNEL_DATA_SEL, GDT_TSS_SEL);

    log_info("gdt: custom GDT and TSS loaded (CS 0x%02x, DS 0x%02x, TR 0x%02x)",
             GDT_KERNEL_CODE_SEL, GDT_KERNEL_DATA_SEL, GDT_TSS_SEL);
    log_info("gdt: tss.rsp0=0x%016lx (bootstrap boot stack)", s_tss.rsp0);
    log_info("gdt: ist1 (#DF)=0x%016lx, ist2 (NMI)=0x%016lx, ist3 (#MC)=0x%016lx",
             s_tss.ist1, s_tss.ist2, s_tss.ist3);
}

const struct tss64 *arch_gdt_get_tss(void)
{
    return &s_tss;
}

const struct gdtr *arch_gdt_get_gdtr(void)
{
    return &s_gdtr;
}
