/*
 * panic.c - fatal error reporting.
 *
 * Contract: no allocation, no locks, serial-only output, works before any other
 * subsystem is initialised. A nested panic (a panic while panicking) skips
 * output and halts immediately to avoid recursion.
 */
#include "kernel/panic.h"

#include <stdarg.h>

#include "kernel/arch.h"
#include "kernel/log.h"

static volatile int panicking;

void panic_at(const char *file, int line, const char *fmt, ...)
{
    arch_irq_disable();

    if (panicking)
        arch_halt_forever();
    panicking = 1;

    kprintf("\n*** KERNEL PANIC ***\n");
    kprintf("at %s:%d\n", file, line);

    va_list ap;
    va_start(ap, fmt);
    kvprintf(fmt, ap);
    va_end(ap);
    kprintf("\nSystem halted.\n");

#ifdef ATAOS_TEST_BUILD
    arch_test_exit(ARCH_TEST_EXIT_FAIL);
#endif
    arch_halt_forever();
}
