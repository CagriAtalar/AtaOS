/*
 * string.c - freestanding memory/string helpers.
 *
 * Deliberately simple byte loops. Built with -fno-builtin so the compiler does
 * not turn these loops back into calls to themselves.
 */
#include "kernel/string.h"

#include <stdint.h>

void *memcpy(void *dst, const void *src, size_t n)
{
    unsigned char *d = dst;
    const unsigned char *s = src;

    while (n--)
        *d++ = *s++;
    return dst;
}

void *memmove(void *dst, const void *src, size_t n)
{
    unsigned char *d = dst;
    const unsigned char *s = src;

    if (d == s || n == 0)
        return dst;

    /* Compare as integers: pointer comparison of unrelated objects is not
     * well-defined in C, and we only need an ordering of the two addresses. */
    if ((uintptr_t)d < (uintptr_t)s) {
        while (n--)
            *d++ = *s++;
    } else {
        d += n;
        s += n;
        while (n--)
            *--d = *--s;
    }
    return dst;
}

void *memset(void *dst, int c, size_t n)
{
    unsigned char *d = dst;

    while (n--)
        *d++ = (unsigned char)c;
    return dst;
}

int memcmp(const void *a, const void *b, size_t n)
{
    const unsigned char *x = a;
    const unsigned char *y = b;

    while (n--) {
        if (*x != *y)
            return (int)*x - (int)*y;
        x++;
        y++;
    }
    return 0;
}

size_t strlen(const char *s)
{
    size_t n = 0;

    while (s[n])
        n++;
    return n;
}

size_t strnlen(const char *s, size_t max)
{
    size_t n = 0;

    while (n < max && s[n])
        n++;
    return n;
}

int strcmp(const char *a, const char *b)
{
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

int strncmp(const char *a, const char *b, size_t n)
{
    while (n && *a && *a == *b) {
        a++;
        b++;
        n--;
    }
    if (n == 0)
        return 0;
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

size_t strlcpy(char *dst, const char *src, size_t dst_size)
{
    size_t len = strlen(src);

    if (dst_size != 0) {
        size_t copy = len < dst_size - 1 ? len : dst_size - 1;
        memcpy(dst, src, copy);
        dst[copy] = '\0';
    }
    return len;
}
