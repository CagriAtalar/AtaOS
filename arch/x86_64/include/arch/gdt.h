/*
 * gdt.h - x86_64 Global Descriptor Table (GDT) and Task State Segment (TSS).
 *
 * Architecture-internal definitions for arch/x86_64/.
 */
#ifndef ATAOS_ARCH_X86_64_GDT_H
#define ATAOS_ARCH_X86_64_GDT_H

#include <stdint.h>
#include "kernel/compiler.h"

/*
 * Segment Selectors (ADR-0008, docs/SYSCALL_ABI.md).
 *
 * Layout:
 *   0x00: Null descriptor
 *   0x08: Kernel 64-bit code
 *   0x10: Kernel 64-bit data
 *   0x18: User 64-bit data (RPL 3 = 0x1B)
 *   0x20: User 64-bit code (RPL 3 = 0x23)
 *   0x28: 64-bit TSS descriptor (16 bytes, covers 0x28 and 0x30)
 */
#define GDT_KERNEL_CODE_SEL 0x08
#define GDT_KERNEL_DATA_SEL 0x10
#define GDT_USER_DATA_SEL   0x18
#define GDT_USER_CODE_SEL   0x20
#define GDT_TSS_SEL         0x28

#define GDT_USER_DATA_RPL3  (GDT_USER_DATA_SEL | 3) /* 0x1B */
#define GDT_USER_CODE_RPL3  (GDT_USER_CODE_SEL | 3) /* 0x23 */

/* IST Stack indices (1-based, 1..7) */
#define GDT_IST_DF          1   /* Double Fault (#DF) */
#define GDT_IST_NMI         2   /* Non-Maskable Interrupt (NMI) */
#define GDT_IST_MC          3   /* Machine Check (#MC) */

#define GDT_IST_STACK_SIZE  4096

/*
 * 64-bit Task State Segment (AMD64 Architecture Programmer's Manual, Vol 2, §12.2.5).
 * Size must be exactly 104 bytes.
 */
struct ATAOS_PACKED tss64 {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iopb_offset;
};
STATIC_ASSERT(sizeof(struct tss64) == 104, "struct tss64 must be exactly 104 bytes");

/*
 * GDTR pointer loaded via `lgdt`.
 */
struct ATAOS_PACKED gdtr {
    uint16_t limit;
    uint64_t base;
};
STATIC_ASSERT(sizeof(struct gdtr) == 10, "struct gdtr must be exactly 10 bytes");

/* Assembly helper to reload GDTR, segment registers, CS, and TR. */
void arch_gdt_flush(const struct gdtr *d, uint16_t code_sel, uint16_t data_sel, uint16_t tss_sel);

/* Diagnostic inspections for tests / verification */
const struct tss64 *arch_gdt_get_tss(void);
const struct gdtr *arch_gdt_get_gdtr(void);

#endif /* ATAOS_ARCH_X86_64_GDT_H */
