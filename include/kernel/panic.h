/*
 * panic.h - panic and assertion support.
 *
 * PANIC() prints a diagnostic (file, line, message) on the early console,
 * disables interrupts and halts the CPU forever. It performs no memory
 * allocation, takes no locks and depends only on the early console, so it
 * works before any other subsystem exists and from any context.
 *
 * ASSERT() checks an internal invariant. It is active in debug builds
 * (ATAOS_DEBUG) and compiled out in release builds; its condition must not have
 * side effects. ASSERT_ALWAYS() is active in every build. Neither is a
 * substitute for validating untrusted input (see docs/SECURITY.md).
 *
 * Future (planned, not implemented): register dump and stack trace using the
 * frame pointers kept by the build (-fno-omit-frame-pointer).
 */
#ifndef ATAOS_KERNEL_PANIC_H
#define ATAOS_KERNEL_PANIC_H

#include "kernel/compiler.h"

ATAOS_NORETURN void panic_at(const char *file, int line, const char *fmt, ...)
    PRINTF_LIKE(3, 4);

#define PANIC(...) panic_at(__FILE__, __LINE__, __VA_ARGS__)

#define ASSERT_ALWAYS(cond)                                                    \
    do {                                                                       \
        if (!(cond))                                                           \
            panic_at(__FILE__, __LINE__, "assertion failed: %s", #cond);       \
    } while (0)

#ifdef ATAOS_DEBUG
#define ASSERT(cond) ASSERT_ALWAYS(cond)
#else
#define ASSERT(cond) ((void)0)
#endif

#endif /* ATAOS_KERNEL_PANIC_H */
