#include "test_framework.h"
#include <kernel/fmt.h>
#include <kernel/string.h>

static bool test_fmt_string_and_char(void) {
    char buf[64];
    size_t n;

    n = ksnprintf(buf, sizeof(buf), "Hello, %s! Char: %c", "World", 'X');
    ASSERT_EQ(n, 21);
    ASSERT_STR_EQ(buf, "Hello, World! Char: X");

    /* Truncation check */
    char small[8];
    const char *long_str = "123456789";
    n = ksnprintf(small, sizeof(small), "%s", long_str);
    ASSERT_EQ(n, 9);
    ASSERT_STR_EQ(small, "1234567");

    /* Zero buffer */
    n = ksnprintf(NULL, 0, "Testing %s", "counting");
    ASSERT_EQ(n, 16);

    /* Null string check */
    const char *null_str = NULL;
    ksnprintf(buf, sizeof(buf), "null=%s", null_str);
    ASSERT_STR_EQ(buf, "null=(null)");

    return true;
}

static bool test_fmt_integers(void) {
    char buf[64];

    ksnprintf(buf, sizeof(buf), "%d %d %d", 0, 42, -12345);
    ASSERT_STR_EQ(buf, "0 42 -12345");

    ksnprintf(buf, sizeof(buf), "Unsigned: %u", 4000000000U);
    ASSERT_STR_EQ(buf, "Unsigned: 4000000000");

    ksnprintf(buf, sizeof(buf), "Hex: 0x%x 0x%lx", 0xABCD, 0x123456789ABCDEF0UL);
    ASSERT_STR_EQ(buf, "Hex: 0xabcd 0x123456789abcdef0");

    ksnprintf(buf, sizeof(buf), "Size: %zu", (size_t)1048576);
    ASSERT_STR_EQ(buf, "Size: 1048576");

    return true;
}

static bool test_fmt_pointer_and_percent(void) {
    char buf[64];

    ksnprintf(buf, sizeof(buf), "ptr: %p, pct: %%", (void *)0x1000);
    ASSERT_STR_EQ(buf, "ptr: 0x1000, pct: %");

    ksnprintf(buf, sizeof(buf), "null: %p", (void *)0);
    ASSERT_STR_EQ(buf, "null: 0x0");

    return true;
}

static bool test_fmt_padding(void) {
    char buf[64];

    ksnprintf(buf, sizeof(buf), "[%04x]", 0x2A);
    ASSERT_STR_EQ(buf, "[002a]");

    ksnprintf(buf, sizeof(buf), "[%08d]", 123);
    ASSERT_STR_EQ(buf, "[00000123]");

    return true;
}

void test_fmt_suite(void) {
    static const test_case_t tests[] = {
        {"strings, chars, null pointer, and buffer truncation", test_fmt_string_and_char},
        {"integers, signed, unsigned, hex, size_t", test_fmt_integers},
        {"pointer formatting and percent escape", test_fmt_pointer_and_percent},
        {"zero padding width formatting", test_fmt_padding},
    };
    run_test_suite("kernel/lib/fmt.c", tests, sizeof(tests) / sizeof(tests[0]));
}
