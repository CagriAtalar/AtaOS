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
