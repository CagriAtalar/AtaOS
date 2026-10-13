/*
 * log.h - kernel logging to the early console.
 *
 * Output goes to the serial console only (M1). Format specifiers are those
 * documented in kernel/fmt.h. Messages are line-oriented: klog() appends the
 * newline. Not yet interrupt-safe (no interrupts are enabled in M1); a lock
 * will be added together with the first interrupt-enabling milestone.
 */
#ifndef ATAOS_KERNEL_LOG_H
#define ATAOS_KERNEL_LOG_H

#include <stdarg.h>

#include "kernel/compiler.h"

enum log_level {
    LOG_ERROR = 0,
    LOG_WARN = 1,
    LOG_INFO = 2,
    LOG_DEBUG = 3,
};

/* Raw formatted output, no prefix, no newline added. */
void kprintf(const char *fmt, ...) PRINTF_LIKE(1, 2);
void kvprintf(const char *fmt, va_list ap);

/* Leveled, prefixed ("[ataos] ") line output; newline is appended. */
void klog(enum log_level level, const char *fmt, ...) PRINTF_LIKE(2, 3);

#define log_error(...) klog(LOG_ERROR, __VA_ARGS__)
#define log_warn(...) klog(LOG_WARN, __VA_ARGS__)
#define log_info(...) klog(LOG_INFO, __VA_ARGS__)
#define log_debug(...) klog(LOG_DEBUG, __VA_ARGS__)

#endif /* ATAOS_KERNEL_LOG_H */
