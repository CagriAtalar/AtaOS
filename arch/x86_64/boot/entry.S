/*
 * entry.S - kernel entry point reached from Limine.
 *
 * Contract on entry (Limine protocol, x86-64): long mode, paging enabled with
 * bootloader page tables, interrupts disabled, SysV ABI. The bootloader stack
 * is bootloader-owned memory of unspecified lifetime, so we immediately switch
 * to our own stack.
 *
 * Registers: all caller state is discarded; RSP/RBP are set here.
 * Stack: 16 KiB boot stack in .bss, no guard page (known limitation, see
 * docs/MEMORY_LAYOUT.md). Replaced by per-thread kernel stacks in M6.
 */
    .section .text.boot, "ax", @progbits
    .global _start
    .type _start, @function
_start:
    cli
    cld
    lea     boot_stack_top(%rip), %rsp
    xor     %ebp, %ebp              /* terminate frame-pointer chains */
    call    kmain
    /* kmain never returns; if it does, halt safely. */
1:  cli
    hlt
    jmp     1b
    .size _start, . - _start

    .section .bss.boot_stack, "aw", @nobits
    .balign 16
    .global boot_stack_bottom
boot_stack_bottom:
    .skip   16384
    .global boot_stack_top
boot_stack_top:

    .section .note.GNU-stack, "", @progbits
