/*
 * idt.c - x86_64 Interrupt Descriptor Table (IDT) initialization.
 *
 * Implements M2 Tasks 2 & 3:
 *   - 256-entry 64-bit IDT table
 *   - Vectors 0-31 wired to per-vector assembly exception stubs
 *   - Vectors 32-255 wired to shared emergency exception stub
 *   - Dedicated IST stacks for vectors 2 (NMI -> IST2), 8 (#DF -> IST1), 18 (#MC -> IST3)
 *   - CPU control register sanity checks (CR0.WP, EFER.NXE)
 *   - IDTR loaded via `lidt`
 *   - Maskable interrupts kept disabled (no `sti`)
 */
#include "arch/idt.h"
#include "arch/gdt.h"
#include "kernel/arch.h"
#include "kernel/compiler.h"
#include "kernel/log.h"
#include "kernel/panic.h"
#include "kernel/string.h"

#define IA32_EFER_MSR 0xC0000080
#define CR0_WP        (1ULL << 16)
#define EFER_NXE      (1ULL << 11)

/* Pointers to the 32 exception entry stubs defined in exception_stubs.S */
extern const uintptr_t x86_exception_stubs[32];

/*
 * Static 256-entry Interrupt Descriptor Table.
 * 16-byte aligned.
 */
static struct idt_entry s_idt[IDT_ENTRIES] ATAOS_ALIGNED(16);

/* IDTR loaded into the CPU */
static struct idtr s_idtr;

static inline uint64_t read_cr0(void)
{
    uint64_t val;
    __asm__ volatile("mov %%cr0, %0" : "=r"(val));
    return val;
}

static inline uint64_t read_msr(uint32_t msr)
{
    uint32_t low, high;
    __asm__ volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32) | low;
}

static void verify_cpu_control_registers(void)
{
    uint64_t cr0 = read_cr0();
    uint64_t efer = read_msr(IA32_EFER_MSR);

    if (!(cr0 & CR0_WP))
        PANIC("CPU sanity check failed: CR0.WP (Write Protect) is not enabled");

    if (!(efer & EFER_NXE))
        PANIC("CPU sanity check failed: EFER.NXE (No-Execute Enable) is not enabled");

    log_info("cpu: control registers verified (CR0.WP 0x%lx, EFER.NXE 0x%lx)",
             (unsigned long)(cr0 & CR0_WP), (unsigned long)(efer & EFER_NXE));
}

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

    /* Verify CPU invariants before table activation */
    verify_cpu_control_registers();

    memset(s_idt, 0, sizeof(s_idt));

    /*
     * Populate descriptors 0-31 with per-vector exception entry stubs, and
     * descriptors 32-255 with the emergency halt stub.
     */
    for (size_t i = 0; i < IDT_ENTRIES; i++) {
        uintptr_t handler = (i < 32) ? x86_exception_stubs[i] : emergency_stub;
        uint8_t ist = 0;

        /* Assign dedicated IST stacks to critical exceptions (ADR-0008) */
        if (i == IDT_VEC_DF)
            ist = GDT_IST_DF; /* IST1 */
        else if (i == IDT_VEC_NMI)
            ist = GDT_IST_NMI; /* IST2 */
        else if (i == IDT_VEC_MC)
            ist = GDT_IST_MC; /* IST3 */

        idt_set_gate(i, handler, GDT_KERNEL_CODE_SEL, ist,
                     IDT_TYPE_INTERRUPT_GATE);
    }

    s_idtr.limit = (uint16_t)(sizeof(s_idt) - 1);
    s_idtr.base = (uint64_t)(uintptr_t)&s_idt[0];

    /* Load the IDT into the CPU */
    arch_idt_load(&s_idtr);

    log_info("idt: IDTR loaded (base 0x%016lx, limit 0x%04x)",
             s_idtr.base, s_idtr.limit);
    log_info("idt: vectors 0-31 exception stubs installed");
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
