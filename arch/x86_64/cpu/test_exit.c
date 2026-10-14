/*
 * test_exit.c - TEST-ONLY QEMU exit via the isa-debug-exit device.
 *
 * Compiled only when the kernel is built with TEST=1 (-DATAOS_TEST_BUILD);
 * the Makefile excludes this file otherwise, so ordinary builds contain no
 * reference to I/O port 0xf4.
 *
 * Device configuration expected by tests/qemu/run.py:
 *     -device isa-debug-exit,iobase=0xf4,iosize=0x04
 * Writing byte v to port 0xf4 makes QEMU exit with status (v << 1) | 1.
 * See ARCH_TEST_EXIT_PASS / ARCH_TEST_EXIT_FAIL in kernel/arch.h.
 */
#ifndef ATAOS_TEST_BUILD
#error "test_exit.c must only be built with -DATAOS_TEST_BUILD"
#endif

#include "arch/io.h"
#include "kernel/arch.h"

#define QEMU_DEBUG_EXIT_PORT 0xf4

void arch_test_exit(int code)
{
    outb(QEMU_DEBUG_EXIT_PORT, (uint8_t)code);
}

void arch_test_fault_de(void)
{
    volatile int zero = 0;
    volatile int num = 100;
    volatile int res = num / zero;
    (void)res;
}

void arch_test_fault_ud(void)
{
    __asm__ volatile("ud2");
}

void arch_test_fault_gp(void)
{
    /* Access to a non-canonical address triggers #GP */
    volatile uint64_t *ptr = (volatile uint64_t *)0xaaaaaaaaaaaaaaaaULL;
    *ptr = 0x42;
}

void arch_test_fault_pf(void)
{
    /* Access to a canonical unmapped page triggers #PF (CR2 populated) */
    volatile uint64_t *ptr = (volatile uint64_t *)0x0000000000000100ULL;
    *ptr = 0x42;
}

void arch_test_fault_df(void)
{
    /*
     * Load RSP with 0 (unmapped). When ud2 executes, the CPU attempts to push
     * the exception frame to address 0, causing a page fault. Because the stack
     * cannot be written, this escalates to a Double Fault (#DF). Since vector 8
     * has IST1 configured in the IDT, the CPU switches to s_df_stack and executes
     * the #DF handler rather than triple faulting.
     */
    __asm__ volatile("movq $0, %%rsp\n\t"
                     "ud2\n\t" : : : "memory");
}
