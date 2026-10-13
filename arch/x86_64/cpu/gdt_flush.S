/*
 * gdt_flush.S - Assembly helper for loading GDT, reloading CS, and loading TR.
 *
 * Prototype (SysV AMD64 ABI):
 *   void arch_gdt_flush(const struct gdtr *gdtr, uint16_t code_sel,
 *                       uint16_t data_sel, uint16_t tss_sel);
 *
 * Parameters:
 *   %rdi: pointer to struct gdtr
 *   %rsi: kernel code segment selector (0x08)
 *   %rdx: kernel data segment selector (0x10)
 *   %rcx: TSS segment selector (0x28)
 */
    .section .text, "ax", @progbits
    .global arch_gdt_flush
    .type arch_gdt_flush, @function
arch_gdt_flush:
    /* Load the Global Descriptor Table Register */
    lgdt    (%rdi)

    /* Reload data segment registers with kernel data selector */
    mov     %dx, %ds
    mov     %dx, %es
    mov     %dx, %ss
    xor     %eax, %eax
    mov     %ax, %fs
    mov     %ax, %gs

    /*
     * Reload CS via a 64-bit far return (lretq).
     * SysV ABI stack layout for lretq:
     *   [RSP + 8] = New Code Segment (CS)
     *   [RSP + 0] = New Instruction Pointer (RIP)
     */
    pushq   %rsi
    leaq    .Lcs_reloaded(%rip), %rax
    pushq   %rax
    lretq

.Lcs_reloaded:
    /* Load the Task Register with the TSS selector */
    ltr     %cx
    ret
    .size arch_gdt_flush, . - arch_gdt_flush

    .section .note.GNU-stack, "", @progbits
