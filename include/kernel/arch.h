/*
 * arch.h - the narrow interface between architecture-independent kernel code
 * and the architecture layer (arch/x86_64/).
 *
 * Architecture-independent code may call these functions; it must not use
 * inline assembly, port I/O or control registers directly.
 *
 * Status: M1 contains only the early-console, halt and test-exit hooks. The
 * interface grows milestone by milestone (see docs/ARCHITECTURE.md).
 */
#ifndef ATAOS_KERNEL_ARCH_H
#define ATAOS_KERNEL_ARCH_H

#include "kernel/compiler.h"

/*
 * Initialise the early serial console. Safe to call before anything else; uses
 * no memory allocation. Best-effort: there is no error return because the
 * panic path must work even if the console is absent.
 */
void arch_early_console_init(void);

/*
 * Initialise the custom x86_64 Global Descriptor Table (GDT) and Task State
 * Segment (TSS). Loads 64-bit kernel and user segment descriptors, points RSP0
 * to the bootstrap stack, populates IST stacks for #DF, NMI, and #MC, and
 * loads the Task Register (TR). Must be called with interrupts disabled.
 */
void arch_gdt_init(void);

/*
 * Initialise the minimal Interrupt Descriptor Table (IDT).
 * Sets up 256 present 64-bit interrupt gates pointing to an emergency exception
 * stub, binds critical exception vectors to dedicated IST stacks (NMI -> IST2,
 * #DF -> IST1, #MC -> IST3), and loads IDTR via `lidt`.
 * Interrupts remain disabled.
 */
void arch_idt_init(void);

/* Write one character to the early console. Callable from any context. */
void arch_early_console_putc(char c);

/* Disable maskable interrupts on the current CPU. */
void arch_irq_disable(void);

/* Disable interrupts and halt the CPU forever. Never returns. */
ATAOS_NORETURN void arch_halt_forever(void);

#ifdef ATAOS_TEST_BUILD
/*
 * Test-only exit codes written to the QEMU isa-debug-exit device (I/O port
 * 0xf4). QEMU turns a written value v into process exit status (v << 1) | 1.
 *   ARCH_TEST_EXIT_PASS (0x10) -> exit status 33
 *   ARCH_TEST_EXIT_FAIL (0x11) -> exit status 35
 * These symbols exist ONLY when the kernel is built with -DATAOS_TEST_BUILD.
 */
#define ARCH_TEST_EXIT_PASS 0x10
#define ARCH_TEST_EXIT_FAIL 0x11

/*
 * Request emulator exit with the given code. If no debug-exit device is
 * present the call returns and the caller must halt.
 */
void arch_test_exit(int code);

/*
 * Deliberately trigger exceptions for testing CPU exception handling (M2):
 *   - #DE: Divide Error (division by zero)
 *   - #UD: Invalid Opcode (via `ud2`)
 *   - #GP: General Protection Fault (non-canonical memory access)
 *   - #PF: Page Fault (unmapped page access)
 *   - #DF: Double Fault (stack exhaustion/corruption switching to IST1)
 */
void arch_test_fault_de(void);
void arch_test_fault_ud(void);
void arch_test_fault_gp(void);
void arch_test_fault_pf(void);
void arch_test_fault_df(void);
#endif

#endif /* ATAOS_KERNEL_ARCH_H */
