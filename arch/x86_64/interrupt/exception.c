/*
 * exception.c - x86_64 CPU exception dispatcher and register dump.
 *
 * Implements M2 Task 3:
 *   - Formatted register dumps on fatal CPU exceptions
 *   - Structured exception reporting (#DE, #UD, #GP, #PF, #DF, etc.)
 *   - Safe halt via arch_halt_forever() (and arch_test_exit in test builds)
 */
#include "arch/page_fault.h"
#include "arch/trap_frame.h"
#include "kernel/arch.h"
#include "kernel/compiler.h"
#include "kernel/log.h"

static const char *const s_exception_names[32] = {
    [0]  = "Divide Error (#DE)",
    [1]  = "Debug (#DB)",
    [2]  = "Non-Maskable Interrupt (NMI)",
    [3]  = "Breakpoint (#BP)",
    [4]  = "Overflow (#OF)",
    [5]  = "Bound Range Exceeded (#BR)",
    [6]  = "Invalid Opcode (#UD)",
    [7]  = "Device Not Available (#NM)",
    [8]  = "Double Fault (#DF)",
    [9]  = "Coprocessor Segment Overrun",
    [10] = "Invalid TSS (#TS)",
    [11] = "Segment Not Present (#NP)",
    [12] = "Stack-Segment Fault (#SS)",
    [13] = "General Protection Fault (#GP)",
    [14] = "Page Fault (#PF)",
    [15] = "Reserved (15)",
    [16] = "x87 FPU Floating-Point Error (#MF)",
    [17] = "Alignment Check (#AC)",
    [18] = "Machine Check (#MC)",
    [19] = "SIMD Floating-Point Exception (#XM)",
    [20] = "Virtualization Exception (#VE)",
    [21] = "Control Protection Exception (#CP)",
    [22] = "Reserved (22)",
    [23] = "Reserved (23)",
    [24] = "Reserved (24)",
    [25] = "Reserved (25)",
    [26] = "Reserved (26)",
    [27] = "Reserved (27)",
    [28] = "Hypervisor Injection Exception (#HV)",
    [29] = "VMM Communication Exception (#VC)",
    [30] = "Security Exception (#SX)",
    [31] = "Reserved (31)",
};

static inline uint64_t read_cr2(void)
{
    uint64_t val;
    __asm__ volatile("mov %%cr2, %0" : "=r"(val));
    return val;
}

static inline uint64_t read_cr3(void)
{
    uint64_t val;
    __asm__ volatile("mov %%cr3, %0" : "=r"(val));
    return val;
}

void x86_exception_dispatch(struct trap_frame *frame);

void x86_exception_dispatch(struct trap_frame *frame)
{
    arch_irq_disable();

    uint64_t cr2 = read_cr2();
    uint64_t cr3 = read_cr3();

    const char *name = (frame->vector < 32 && s_exception_names[frame->vector])
                           ? s_exception_names[frame->vector]
                           : "Unknown Exception";

    kprintf("\n*** CPU EXCEPTION ***\n");
    kprintf("Vector: %lu (%s), Error Code: 0x%lx\n", frame->vector, name, frame->error_code);
    kprintf("RIP: 0x%016lx  CS: 0x%04lx  RFLAGS: 0x%016lx\n", frame->rip, frame->cs, frame->rflags);
    kprintf("RSP: 0x%016lx  SS: 0x%04lx  CR2: 0x%016lx  CR3: 0x%016lx\n",
            frame->rsp, frame->ss, cr2, cr3);
    kprintf("RAX: 0x%016lx  RBX: 0x%016lx  RCX: 0x%016lx  RDX: 0x%016lx\n",
            frame->rax, frame->rbx, frame->rcx, frame->rdx);
    kprintf("RSI: 0x%016lx  RDI: 0x%016lx  RBP: 0x%016lx\n",
            frame->rsi, frame->rdi, frame->rbp);
    kprintf("R8 : 0x%016lx  R9 : 0x%016lx  R10: 0x%016lx  R11: 0x%016lx\n",
            frame->r8, frame->r9, frame->r10, frame->r11);
    kprintf("R12: 0x%016lx  R13: 0x%016lx  R14: 0x%016lx  R15: 0x%016lx\n",
            frame->r12, frame->r13, frame->r14, frame->r15);

    if (frame->vector == 14) {
        struct x86_page_fault pf = x86_decode_page_fault(frame->error_code);

        kprintf("PF: CR2=0x%016lx origin=%s access=%s cause=%s fetch=%s\n", cr2,
                x86_pf_origin(&pf), x86_pf_access(&pf), x86_pf_cause(&pf),
                x86_pf_fetch_kind(&pf));
        kprintf("PF: present=%u write=%u user=%u reserved=%u pk=%u sgx=%u class=%s\n",
                pf.present, pf.write, pf.user, pf.reserved, pf.protection_key,
                pf.shadow_stack, pf.user ? "user-demand" : "supervisor-fatal");
        if (pf.present && pf.write && !pf.user)
            kprintf("PF: hint=kernel write to a read-only/W^X-protected page\n");
        else if (pf.present && pf.instruction_fetch)
            kprintf("PF: hint=instruction fetch from an NX page\n");
        else if (!pf.present)
            kprintf("PF: hint=translation absent (demand fault; fatal in M4 kernel)\n");
        else if (pf.reserved)
            kprintf("PF: hint=reserved bit set in a paging structure\n");
    }

    kprintf("System halted.\n");

#ifdef ATAOS_TEST_BUILD
    arch_test_exit(ARCH_TEST_EXIT_FAIL);
#endif
    arch_halt_forever();
}
