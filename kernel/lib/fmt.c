/*
 * fmt.c - minimal freestanding formatter. See include/kernel/fmt.h for the
 * supported syntax and the behaviour for unsupported conversions.
 */
#include "kernel/fmt.h"

#include <stdbool.h>
#include <stdint.h>

#define FMT_MAX_WIDTH 255

struct fmt_state {
    fmt_putc_fn put;
    void *ctx;
    size_t count;
};

static void emit(struct fmt_state *st, char c)
{
    st->put(c, st->ctx);
    st->count++;
}

static void emit_pad(struct fmt_state *st, char c, size_t n)
{
    while (n--)
        emit(st, c);
}

/*
 * Emit a number given as digits already produced in reverse order in
 * rev[0..len). 'prefix' is "-" / "0x" / "" and is written before zero padding.
 */
static void emit_number(struct fmt_state *st, const char *rev, size_t len, const char *prefix,
                        size_t width, bool zero, bool left)
{
    size_t plen = 0;
    while (prefix[plen])
        plen++;

    size_t total = plen + len;
    size_t pad = width > total ? width - total : 0;

    if (!left && !zero)
        emit_pad(st, ' ', pad);
    for (size_t i = 0; i < plen; i++)
        emit(st, prefix[i]);
    if (!left && zero)
        emit_pad(st, '0', pad);
    while (len--)
        emit(st, rev[len]);
    if (left)
        emit_pad(st, ' ', pad);
}

static size_t to_digits(char *rev, uint64_t v, unsigned base, bool upper)
{
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    size_t n = 0;

    do {
        rev[n++] = digits[v % base];
        v /= base;
    } while (v != 0);
    return n;
}

size_t kfmt_vformat(fmt_putc_fn put, void *ctx, const char *fmt, va_list ap)
{
    struct fmt_state st = {.put = put, .ctx = ctx, .count = 0};

    for (; *fmt; fmt++) {
        if (*fmt != '%') {
            emit(&st, *fmt);
            continue;
        }
        fmt++;

        bool zero = false;
        bool left = false;
        for (;; fmt++) {
            if (*fmt == '0')
                zero = true;
            else if (*fmt == '-')
                left = true;
            else
                break;
        }

        size_t width = 0;
        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (size_t)(*fmt - '0');
            if (width > FMT_MAX_WIDTH)
                width = FMT_MAX_WIDTH;
            fmt++;
        }

        int lng = 0; /* 0 = int, 1 = long, 2 = long long, 3 = size_t */
        if (*fmt == 'l') {
            lng = 1;
            fmt++;
            if (*fmt == 'l') {
                lng = 2;
                fmt++;
            }
        } else if (*fmt == 'z') {
            lng = 3;
            fmt++;
        }

        char c = *fmt;
        if (c == '\0') {
            emit(&st, '%');
            break;
        }

        char rev[24];
        switch (c) {
        case '%':
            emit(&st, '%');
            break;
        case 'c': {
            char ch = (char)va_arg(ap, int);
            if (!left)
                emit_pad(&st, ' ', width > 1 ? width - 1 : 0);
            emit(&st, ch);
            if (left)
                emit_pad(&st, ' ', width > 1 ? width - 1 : 0);
            break;
        }
        case 's': {
            const char *s = va_arg(ap, const char *);
            if (s == NULL)
                s = "(null)";
            size_t len = 0;
            while (s[len])
                len++;
            size_t pad = width > len ? width - len : 0;
            if (!left)
                emit_pad(&st, ' ', pad);
            for (size_t i = 0; i < len; i++)
                emit(&st, s[i]);
            if (left)
                emit_pad(&st, ' ', pad);
            break;
        }
        case 'd': {
            int64_t sv;
            if (lng == 0)
                sv = va_arg(ap, int);
            else if (lng == 1)
                sv = va_arg(ap, long);
            else if (lng == 2)
                sv = va_arg(ap, long long);
            else
                sv = (int64_t)va_arg(ap, ptrdiff_t); /* %zd: signed size */
            bool neg = sv < 0;
            uint64_t mag = neg ? (uint64_t)0 - (uint64_t)sv : (uint64_t)sv;
            size_t n = to_digits(rev, mag, 10, false);
            emit_number(&st, rev, n, neg ? "-" : "", width, zero && !left, left);
            break;
        }
        case 'u':
        case 'x':
        case 'X': {
            uint64_t uv;
            if (lng == 0)
                uv = va_arg(ap, unsigned int);
            else if (lng == 1)
                uv = va_arg(ap, unsigned long);
            else if (lng == 2)
                uv = va_arg(ap, unsigned long long);
            else
                uv = va_arg(ap, size_t);
            size_t n = to_digits(rev, uv, c == 'u' ? 10 : 16, c == 'X');
            emit_number(&st, rev, n, "", width, zero && !left, left);
            break;
        }
        case 'p': {
            uint64_t uv = (uint64_t)(uintptr_t)va_arg(ap, void *);
            size_t n = to_digits(rev, uv, 16, false);
            emit_number(&st, rev, n, "0x", width, zero && !left, left);
            break;
        }
        default:
            /* Unsupported conversion: emit it literally; consume no argument. */
            emit(&st, '%');
            emit(&st, c);
            break;
        }
    }
    return st.count;
}

struct snprintf_ctx {
    char *buf;
    size_t size;
    size_t pos;
};

static void snprintf_put(char c, void *ctx)
{
    struct snprintf_ctx *s = ctx;

    if (s->size != 0 && s->pos < s->size - 1)
        s->buf[s->pos] = c;
    s->pos++;
}

size_t kvsnprintf(char *buf, size_t size, const char *fmt, va_list ap)
{
    struct snprintf_ctx ctx = {.buf = buf, .size = size, .pos = 0};
    size_t n = kfmt_vformat(snprintf_put, &ctx, fmt, ap);

    if (size != 0)
        buf[ctx.pos < size - 1 ? ctx.pos : size - 1] = '\0';
    return n;
}

size_t ksnprintf(char *buf, size_t size, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    size_t n = kvsnprintf(buf, size, fmt, ap);
    va_end(ap);
    return n;
}
