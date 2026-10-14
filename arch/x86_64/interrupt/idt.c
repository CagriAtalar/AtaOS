/*
 * idt.c - x86_64 Interrupt Descriptor Table (IDT) initialization.
 *
 * Implements M2 Task 2:
 *   - 256-entry 64-bit IDT table
 *   - Valid present 64-bit interrupt-gate descriptors (0x8E)
 *   - Shared non-returning emergency exception stub
 *   - IST stack binding for vectors 2 (NMI -> IST2), 8 (#DF -> IST1), 18 (#MC -> IST3)
 *   - IDTR loaded via `lidt`
 *   - Maskable interrupts kept disabled (no `sti`)
 */
#include "arch/idt.h"
#include "arch/gdt.h"
#include "kernel/arch.h"
#include "kernel/compiler.h"
#include "kernel/log.h"
#include "kernel/string.h"

/*
 * Static 256-entry Interrupt Descriptor Table.
 * 16-byte aligned.
 */
static struct idt_entry s_idt[IDT_ENTRIES] ATAOS_ALIGNED(16);

/* IDTR loaded into the CPU */
static struct idtr s_idtr;

static void idt_set_gate(size_t vector, uintptr_t handler, uint16_t selector,
                         uint8_t ist, uint8_t type_attr)
{
    if (vector >= IDT_ENTRIES)
        return;

    s_idt[vector].offset_low = (uint16_t)(handler & 0xFFFFULL);
    s_idt[vector].selector = selector;
    s_idt[vector].ist = (uint8_t)(ist & 0x07);
    s_idt[vector].type_attr = type_attr;
    s_idt[vector].offset_mid = (uint16_t)((handler >> 16) & 0xFFFFULL);
    s_idt[vector].offset_high = (uint32_t)((handler >> 32) & 0xFFFFFFFFULL);
    s_idt[vector].reserved = 0;
}

void arch_idt_init(void)
{
    uintptr_t emergency_stub = (uintptr_t)arch_emergency_exception_stub;

    memset(s_idt, 0, sizeof(s_idt));

    /*
     * Populate all 256 descriptors as present 64-bit interrupt gates pointing
     * to the emergency halt stub.
     */
    for (size_t i = 0; i < IDT_ENTRIES; i++) {
        uint8_t ist = 0;

        /* Assign dedicated IST stacks to critical exceptions (ADR-0008) */
        if (i == IDT_VEC_DF)
            ist = GDT_IST_DF; /* IST1 */
        else if (i == IDT_VEC_NMI)
            ist = GDT_IST_NMI; /* IST2 */
        else if (i == IDT_VEC_MC)
            ist = GDT_IST_MC; /* IST3 */

        idt_set_gate(i, emergency_stub, GDT_KERNEL_CODE_SEL, ist,
                     IDT_TYPE_INTERRUPT_GATE);
    }

    s_idtr.limit = (uint16_t)(sizeof(s_idt) - 1);
    s_idtr.base = (uint64_t)(uintptr_t)&s_idt[0];

    /* Load the IDT into the CPU */
    arch_idt_load(&s_idtr);

    log_info("idt: minimal 256-entry IDT loaded (base 0x%016lx, limit 0x%04x)",
             s_idtr.base, s_idtr.limit);
    log_info("idt: all gates -> emergency stub at 0x%016lx", (uint64_t)emergency_stub);
    log_info("idt: ist1 (#DF)=vec %u, ist2 (NMI)=vec %u, ist3 (#MC)=vec %u",
             IDT_VEC_DF, IDT_VEC_NMI, IDT_VEC_MC);
}

const struct idtr *arch_idt_get_idtr(void)
{
    return &s_idtr;
}

const struct idt_entry *arch_idt_get_entry(size_t vector)
{
    if (vector >= IDT_ENTRIES)
        return NULL;
    return &s_idt[vector];
}
