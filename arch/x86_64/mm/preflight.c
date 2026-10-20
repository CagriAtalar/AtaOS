/*
 * preflight.c - expose addresses needed by the M4 CR3 pre-flight check.
 *
 * The values are read from the already-initialised GDT/TSS/IDT and from the
 * linker-defined boot stack. No page-table bits are interpreted here.
 */
#include "arch/gdt.h"
#include "arch/idt.h"
#include "kernel/arch.h"
#include "kernel/panic.h"

extern char boot_stack_guard[];
extern char boot_stack_bottom[];
extern char boot_stack_top[];
extern const uintptr_t x86_exception_stubs[32];

void arch_mm_kernel_tables(struct arch_mm_kernel_tables *out)
{
    ASSERT_ALWAYS(out != NULL);

    const struct gdtr *gdtr = arch_gdt_get_gdtr();
    const struct idtr *idtr = arch_idt_get_idtr();
    const struct tss64 *tss = arch_gdt_get_tss();

    out->gdt_base = (uintptr_t)gdtr->base;
    out->gdt_size = (size_t)gdtr->limit + 1;
    out->idt_base = (uintptr_t)idtr->base;
    out->idt_size = (size_t)idtr->limit + 1;
    out->tss_base = (uintptr_t)tss;
    out->tss_size = sizeof(*tss);
    out->ist_top[0] = (uintptr_t)tss->ist1;
    out->ist_top[1] = (uintptr_t)tss->ist2;
    out->ist_top[2] = (uintptr_t)tss->ist3;
}

uintptr_t arch_mm_read_rsp(void)
{
    uintptr_t rsp;
    __asm__ volatile("mov %%rsp, %0" : "=r"(rsp));
    return rsp;
}

void arch_mm_boot_stack_bounds(uintptr_t *low, uintptr_t *high)
{
    ASSERT_ALWAYS(low != NULL && high != NULL);
    *low = (uintptr_t)boot_stack_bottom;
    *high = (uintptr_t)boot_stack_top;
}

uintptr_t arch_mm_boot_stack_guard_addr(void)
{
    return (uintptr_t)boot_stack_guard;
}

uintptr_t arch_mm_ist_guard_addr(unsigned index)
{
    return arch_gdt_ist_guard(index);
}

uintptr_t arch_mm_exception_dispatch_addr(void)
{
    return (uintptr_t)&x86_exception_dispatch;
}

uintptr_t arch_mm_exception_stub_addr(unsigned vector)
{
    ASSERT_ALWAYS(vector < 32);
    return x86_exception_stubs[vector];
}
