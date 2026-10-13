/*
 * fmt.h - minimal freestanding formatted-output engine.
 *
 * Supported conversions:   %s %c %d %u %x %X %p %%
 * Supported length mods:   (none)  l  ll  z        (e.g. %ld %lu %lx %llu %zu %zx)
 * Supported flags/width:   '0' (zero pad), '-' (left justify), decimal width.
 *
 * Unsupported (documented behaviour, never undefined): precision (%.3s),
 * '*' width, %f/%e/%g, %o, %i, %n, 'h'/'hh'/'j'/'t' modifiers and any unknown
 * conversion. For an unsupported conversion character C the output is the two
 * characters '%' and C (flags/width/length already consumed are dropped); a
 * trailing lone '%' at end of string is emitted as '%'. Arguments are NOT
 * consumed for unsupported conversions.
 *
 * %s with a NULL pointer prints "(null)". %p prints "0x" followed by the
 * lowercase hex value without zero padding ("0x0" for NULL).
 *
 * Pure code: no globals, no allocation, no dependencies beyond <stdarg.h>.
 * Host-tested.
 */
#ifndef ATAOS_KERNEL_FMT_H
#define ATAOS_KERNEL_FMT_H

#include <stdarg.h>
#include <stddef.h>

#include "kernel/compiler.h"

/* Output sink: receives one character at a time. */
typedef void (*fmt_putc_fn)(char c, void *ctx);

/* Format into a sink. Returns the number of characters emitted. */
size_t kfmt_vformat(fmt_putc_fn put, void *ctx, const char *fmt, va_list ap);

/*
 * snprintf-like helpers. Write at most size-1 characters plus a NUL (nothing
 * if size == 0). Return the number of characters the full output contains
 * (like C99 snprintf); truncation occurred iff the result >= size.
 */
size_t kvsnprintf(char *buf, size_t size, const char *fmt, va_list ap);
size_t ksnprintf(char *buf, size_t size, const char *fmt, ...) PRINTF_LIKE(3, 4);

#endif /* ATAOS_KERNEL_FMT_H */
