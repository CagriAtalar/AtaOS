/*
 * log.c - kernel logging over the early console.
 */
#include "kernel/log.h"

#include "kernel/arch.h"
#include "kernel/fmt.h"

#ifdef ATAOS_DEBUG
static const enum log_level max_level = LOG_DEBUG;
#else
static const enum log_level max_level = LOG_INFO;
#endif

static void console_put(char c, void *ctx)
{
    (void)ctx;
    arch_early_console_putc(c);
}

void kvprintf(const char *fmt, va_list ap)
{
    (void)kfmt_vformat(console_put, NULL, fmt, ap);
}

void kprintf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    kvprintf(fmt, ap);
    va_end(ap);
}

void klog(enum log_level level, const char *fmt, ...)
{
    static const char *const prefix[] = {
        [LOG_ERROR] = "[ataos] error: ",
        [LOG_WARN] = "[ataos] warn: ",
        [LOG_INFO] = "[ataos] ",
        [LOG_DEBUG] = "[ataos] debug: ",
    };

    if (level > max_level)
        return;

    kprintf("%s", prefix[level]);

    va_list ap;
    va_start(ap, fmt);
    kvprintf(fmt, ap);
    va_end(ap);

    arch_early_console_putc('\n');
}
