/*
 * compiler.h - compiler attributes and compile-time helpers.
 *
 * Usable by kernel code, architecture code and host-side tests. Contains no
 * architecture-specific or hosted-libc dependencies.
 */
#ifndef ATAOS_KERNEL_COMPILER_H
#define ATAOS_KERNEL_COMPILER_H

#include <stddef.h>
#include <stdint.h>

#define ATAOS_NORETURN __attribute__((noreturn))
#define WARN_UNUSED __attribute__((warn_unused_result))
#define PRINTF_LIKE(fmt_idx, args_idx) __attribute__((format(printf, fmt_idx, args_idx)))
#define ATAOS_PACKED __attribute__((packed))
#define ATAOS_ALIGNED(n) __attribute__((aligned(n)))
#define ATAOS_USED __attribute__((used))
#define ATAOS_UNUSED __attribute__((unused))
#define ATAOS_SECTION(name) __attribute__((section(name)))

#define STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

#define KIB (1024ULL)
#define MIB (1024ULL * KIB)
#define GIB (1024ULL * MIB)

#endif /* ATAOS_KERNEL_COMPILER_H */
