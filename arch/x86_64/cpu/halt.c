/*
 * halt.c - interrupt masking and halting.
 */
#include "kernel/arch.h"

void arch_irq_disable(void)
{
    __asm__ volatile("cli" : : : "memory");
}

void arch_halt_forever(void)
{
    for (;;)
        __asm__ volatile("cli; hlt" : : : "memory");
}
