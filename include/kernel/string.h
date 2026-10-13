/*
 * string.h - freestanding memory and string helpers.
 *
 * Implemented in kernel/lib/string.c. The compiler may emit calls to
 * memcpy/memmove/memset/memcmp on its own, so these exact symbol names must
 * exist in the kernel image.
 *
 * When built for host tests (ATAOS_HOST_TEST) the functions are renamed with
 * an "ataos_" prefix so they cannot collide with, or be silently replaced by,
 * the host C library.
 */
#ifndef ATAOS_KERNEL_STRING_H
#define ATAOS_KERNEL_STRING_H

#include <stddef.h>

#ifdef ATAOS_HOST_TEST
#define memcpy ataos_memcpy
#define memmove ataos_memmove
#define memset ataos_memset
#define memcmp ataos_memcmp
#define strlen ataos_strlen
#define strnlen ataos_strnlen
#define strcmp ataos_strcmp
#define strncmp ataos_strncmp
#define strlcpy ataos_strlcpy
#endif

/* Copy n bytes; regions must not overlap. Returns dst. */
void *memcpy(void *dst, const void *src, size_t n);

/* Copy n bytes; regions may overlap. Returns dst. */
void *memmove(void *dst, const void *src, size_t n);

/* Set n bytes to (unsigned char)c. Returns dst. */
void *memset(void *dst, int c, size_t n);

/* Compare n bytes as unsigned char; returns <0, 0, >0. */
int memcmp(const void *a, const void *b, size_t n);

/* Length of NUL-terminated string. */
size_t strlen(const char *s);

/* Length of s, looking at no more than max bytes. */
size_t strnlen(const char *s, size_t max);

/* Compare NUL-terminated strings as unsigned char; returns <0, 0, >0. */
int strcmp(const char *a, const char *b);

/* Compare at most n characters. */
int strncmp(const char *a, const char *b, size_t n);

/*
 * Copy src into dst of size dst_size, always NUL-terminating when
 * dst_size > 0. Returns strlen(src); truncation occurred iff the result is
 * >= dst_size.
 */
size_t strlcpy(char *dst, const char *src, size_t dst_size);

#endif /* ATAOS_KERNEL_STRING_H */
