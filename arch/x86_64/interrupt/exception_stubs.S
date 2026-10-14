/*
 * exception_stubs.S - Assembly entry stubs for CPU exceptions (vectors 0-31).
 *
 * M2: Interrupt Descriptor Table (IDT) and exception dispatcher entry.
 */
    .section .text, "ax", @progbits

    /* Stubs for exceptions without a CPU-pushed error code: push dummy 0 */
.macro ISR_NOERRCODE vec
    .global isr_stub_\vec
    .type isr_stub_\vec, @function
isr_stub_\vec:
    pushq   $0
    pushq   $\vec
    jmp     isr_common_stub
    .size isr_stub_\vec, . - isr_stub_\vec
.endm

    /* Stubs for exceptions with a CPU-pushed error code: CPU already pushed code */
.macro ISR_ERRCODE vec
    .global isr_stub_\vec
    .type isr_stub_\vec, @function
isr_stub_\vec:
    pushq   $\vec
    jmp     isr_common_stub
    .size isr_stub_\vec, . - isr_stub_\vec
.endm

ISR_NOERRCODE 0     /* #DE: Divide Error */
ISR_NOERRCODE 1     /* #DB: Debug */
ISR_NOERRCODE 2     /* NMI: Non-Maskable Interrupt */
ISR_NOERRCODE 3     /* #BP: Breakpoint */
ISR_NOERRCODE 4     /* #OF: Overflow */
ISR_NOERRCODE 5     /* #BR: Bound Range Exceeded */
ISR_NOERRCODE 6     /* #UD: Invalid Opcode */
ISR_NOERRCODE 7     /* #NM: Device Not Available */
ISR_ERRCODE   8     /* #DF: Double Fault */
ISR_NOERRCODE 9     /* Coprocessor Segment Overrun */
ISR_ERRCODE   10    /* #TS: Invalid TSS */
ISR_ERRCODE   11    /* #NP: Segment Not Present */
ISR_ERRCODE   12    /* #SS: Stack Fault */
ISR_ERRCODE   13    /* #GP: General Protection Fault */
ISR_ERRCODE   14    /* #PF: Page Fault */
ISR_NOERRCODE 15    /* Reserved */
ISR_NOERRCODE 16    /* #MF: x87 FPU Floating-Point */
ISR_ERRCODE   17    /* #AC: Alignment Check */
ISR_NOERRCODE 18    /* #MC: Machine Check */
ISR_NOERRCODE 19    /* #XM: SIMD Floating-Point */
ISR_NOERRCODE 20    /* #VE: Virtualization Exception */
ISR_ERRCODE   21    /* #CP: Control Protection Exception */
ISR_NOERRCODE 22    /* Reserved */
ISR_NOERRCODE 23    /* Reserved */
ISR_NOERRCODE 24    /* Reserved */
ISR_NOERRCODE 25    /* Reserved */
ISR_NOERRCODE 26    /* Reserved */
ISR_NOERRCODE 27    /* Reserved */
ISR_NOERRCODE 28    /* #HV: Hypervisor Injection */
ISR_ERRCODE   29    /* #VC: VMM Communication */
ISR_ERRCODE   30    /* #SX: Security Exception */
ISR_NOERRCODE 31    /* Reserved */

    /*
     * Common ISR stub. Saves all general-purpose registers to form
     * `struct trap_frame`, sets direction flag to forward, calls C dispatcher,
     * and restores registers on return.
     */
    .type isr_common_stub, @function
isr_common_stub:
    cld
    pushq   %r15
    pushq   %r14
    pushq   %r13
    pushq   %r12
    pushq   %r11
    pushq   %r10
    pushq   %r9
    pushq   %r8
    pushq   %rbp
    pushq   %rdi
    pushq   %rsi
    pushq   %rdx
    pushq   %rcx
    pushq   %rbx
    pushq   %rax

    /* Pass pointer to `struct trap_frame` as first argument (%rdi) */
    movq    %rsp, %rdi
    call    x86_exception_dispatch

    /* If exception handler returns: restore registers and iretq */
    popq    %rax
    popq    %rbx
    popq    %rcx
    popq    %rdx
    popq    %rsi
    popq    %rdi
    popq    %rbp
    popq    %r8
    popq    %r9
    popq    %r10
    popq    %r11
    popq    %r12
    popq    %r13
    popq    %r14
    popq    %r15

    /* Pop vector number and error code */
    addq    $16, %rsp
    iretq
    .size isr_common_stub, . - isr_common_stub

    /*
     * Array of pointers to all 32 exception entry stubs for IDT initialization.
     */
    .section .rodata
    .global x86_exception_stubs
    .align 8
x86_exception_stubs:
    .quad isr_stub_0,  isr_stub_1,  isr_stub_2,  isr_stub_3
    .quad isr_stub_4,  isr_stub_5,  isr_stub_6,  isr_stub_7
    .quad isr_stub_8,  isr_stub_9,  isr_stub_10, isr_stub_11
    .quad isr_stub_12, isr_stub_13, isr_stub_14, isr_stub_15
    .quad isr_stub_16, isr_stub_17, isr_stub_18, isr_stub_19
    .quad isr_stub_20, isr_stub_21, isr_stub_22, isr_stub_23
    .quad isr_stub_24, isr_stub_25, isr_stub_26, isr_stub_27
    .quad isr_stub_28, isr_stub_29, isr_stub_30, isr_stub_31
    .size x86_exception_stubs, . - x86_exception_stubs

    .section .note.GNU-stack, "", @progbits
