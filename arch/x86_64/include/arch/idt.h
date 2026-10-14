/*
 * idt.h - x86_64 Interrupt Descriptor Table (IDT).
 *
 * Architecture-internal definitions for arch/x86_64/.
 */
#ifndef ATAOS_ARCH_X86_64_IDT_H
#define ATAOS_ARCH_X86_64_IDT_H

#include <stddef.h>
#include <stdint.h>
#include "kernel/compiler.h"

#define IDT_ENTRIES 256

/* Gate Types and Attributes */
#define IDT_TYPE_INTERRUPT_GATE 0x8E /* Present=1, DPL=00b, Type=0xE (64-bit Interrupt Gate) */

/* Well-known exception vectors with dedicated IST stacks (ADR-0008) */
#define IDT_VEC_NMI 2  /* Non-Maskable Interrupt (IST2) */
#define IDT_VEC_DF  8  /* Double Fault (IST1) */
#define IDT_VEC_MC  18 /* Machine Check (IST3) */

/*
 * 64-bit Interrupt Gate Descriptor (AMD64 Architecture Programmer's Manual, Vol 2, §4.8.3).
 * Size must be exactly 16 bytes.
 */
struct ATAOS_PACKED idt_entry {
    uint16_t offset_low;  /* Target RIP bits 15:0 */
    uint16_t selector;    /* Target Code Segment Selector (GDT_KERNEL_CODE_SEL) */
    uint8_t ist;          /* IST index (bits 2:0, 1-based, 0=none); bits 7:3 reserved 0 */
    uint8_t type_attr;    /* Type and Attributes (P=1, DPL=00, Type=0xE) */
    uint16_t offset_mid;  /* Target RIP bits 31:16 */
    uint32_t offset_high; /* Target RIP bits 63:32 */
    uint32_t reserved;    /* Reserved (must be zero) */
};
STATIC_ASSERT(sizeof(struct idt_entry) == 16, "struct idt_entry must be exactly 16 bytes");

/*
 * IDTR pointer loaded via `lidt`.
 * Limit is table size in bytes minus 1 (AMD64 Vol 2, §4.8.2).
 */
struct ATAOS_PACKED idtr {
    uint16_t limit;
    uint64_t base;
};
STATIC_ASSERT(sizeof(struct idtr) == 10, "struct idtr must be exactly 10 bytes");

/* Assembly helper: loads IDTR via `lidt` */
void arch_idt_load(const struct idtr *idtr);

/* Shared emergency exception stub: halts permanently with interrupts disabled */
ATAOS_NORETURN void arch_emergency_exception_stub(void);

/* Diagnostic inspections for tests / verification */
const struct idtr *arch_idt_get_idtr(void);
const struct idt_entry *arch_idt_get_entry(size_t vector);

#endif /* ATAOS_ARCH_X86_64_IDT_H */
