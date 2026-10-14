/*
 * trap_frame.h - x86_64 CPU trap and interrupt frame definition.
 *
 * Architecture-internal structure passed to x86_exception_dispatch().
 */
#ifndef ATAOS_ARCH_X86_64_TRAP_FRAME_H
#define ATAOS_ARCH_X86_64_TRAP_FRAME_H

#include <stdint.h>
#include "kernel/compiler.h"

/*
 * Stack frame constructed by hardware and ISR stubs on exception entry.
 * Size: exactly 176 bytes (22 x 8-byte quadwords).
 */
struct ATAOS_PACKED trap_frame {
    /* General-purpose registers pushed by isr_common_stub (reverse push order) */
    uint64_t rax;
    uint64_t rbx;
    uint64_t rcx;
    uint64_t rdx;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t rbp;
    uint64_t r8;
    uint64_t r9;
    uint64_t r10;
    uint64_t r11;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;

    /* Vector number and error code pushed by per-vector assembly stub */
    uint64_t vector;
    uint64_t error_code;

    /* Hardware exception frame pushed automatically by CPU */
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
};
STATIC_ASSERT(sizeof(struct trap_frame) == 176, "struct trap_frame must be exactly 176 bytes");

#endif /* ATAOS_ARCH_X86_64_TRAP_FRAME_H */
