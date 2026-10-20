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

/* Defined by arch/x86_64/linker.ld. */
extern char __text_start[];
extern char __data_start[];

void arch_test_fault_pfwp(void)
{
    /* Text is mapped RX by vmm_init(); writing it must raise a protection #PF. */
    volatile uint8_t *ptr = (volatile uint8_t *)__text_start;
    *ptr = 0x90;
}

void arch_test_fault_pfnx(void)
{
    /* Data is mapped RW+NX by vmm_init(); fetching it must raise an NX #PF. */
    __asm__ volatile("call *%0" : : "r"((uintptr_t)__data_start) : "memory");
}

void arch_test_fault_df(void)
{
    /*
     * Load RSP with 0 (unmapped). When ud2 executes, the CPU attempts to push
     * the exception frame to address 0, causing a page fault. Because the stack
     * cannot be written, this escalates to a Double Fault (#DF). Since vector 8
     * has IST1 configured in the IDT, the CPU switches to the IST1 stack and
     * executes the #DF handler rather than triple faulting.
     */
    __asm__ volatile("movq $0, %%rsp\n\t"
                     "ud2\n\t" : : : "memory");
}

extern char boot_stack_bottom[];

void arch_test_fault_stackovf(void)
{
    /*
     * Point RSP at the first byte above the boot-stack guard page and push.
     * The push lands in the guard, whose translation vmm_init() removed. The
     * #PF frame also cannot be pushed on the guard, so the CPU escalates to
     * #DF on IST1.
     */
    __asm__ volatile("mov %0, %%rsp\n\t"
                     "pushq $0x42\n\t"
                     :
                     : "r"((uintptr_t)boot_stack_bottom)
                     : "memory");
    __builtin_unreachable();
}
