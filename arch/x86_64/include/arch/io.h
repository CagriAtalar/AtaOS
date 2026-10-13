/*
 * io.h - x86 port I/O primitives. Private to arch/x86_64/.
 */
#ifndef ATAOS_ARCH_X86_64_IO_H
#define ATAOS_ARCH_X86_64_IO_H

#include <stdint.h>

static inline void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port) : "memory");
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port) : "memory");
    return value;
}

#endif /* ATAOS_ARCH_X86_64_IO_H */
