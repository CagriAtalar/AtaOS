/*
 * idt_stub.S - Assembly helpers for minimal IDT loading and emergency halt.
 *
 * M2 Task 2: Minimal safe IDT emergency path.
 */
    .section .text, "ax", @progbits

    /*
     * arch_idt_load - Load the Interrupt Descriptor Table Register (IDTR).
     *
     * Prototype (SysV AMD64 ABI):
     *   void arch_idt_load(const struct idtr *idtr);
     *
     * Parameters:
     *   %rdi: pointer to struct idtr
     */
    .global arch_idt_load
    .type arch_idt_load, @function
arch_idt_load:
    lidt    (%rdi)
    ret
    .size arch_idt_load, . - arch_idt_load

    /*
     * arch_emergency_exception_stub - Shared emergency exception path.
     *
     * In M2 Task 2, all 256 IDT gates point to this stub. It immediately
     * disables maskable interrupts and halts the CPU in a tight loop.
     * It must NEVER attempt to return from an exception (no iretq).
     */
    .global arch_emergency_exception_stub
    .type arch_emergency_exception_stub, @function
arch_emergency_exception_stub:
    cli
.Lemergency_halt:
    hlt
    jmp     .Lemergency_halt
    .size arch_emergency_exception_stub, . - arch_emergency_exception_stub

    .section .note.GNU-stack, "", @progbits
