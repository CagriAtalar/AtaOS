#include "test_framework.h"
#include <kernel/string.h>

static bool test_memset_basic(void) {
    char buf[16];
    for (int i = 0; i < 16; i++) buf[i] = (char)0xAA;
    
    memset(buf, 0, 16);
    for (int i = 0; i < 16; i++) {
        ASSERT_EQ(buf[i], 0);
    }

    memset(buf + 4, 0x55, 8);
    ASSERT_EQ(buf[3], 0);
    for (int i = 4; i < 12; i++) {
        ASSERT_EQ(buf[i], 0x55);
    }
    ASSERT_EQ(buf[12], 0);

    /* Zero length should be a no-op */
    void *ret = memset(buf, 0xFF, 0);
    ASSERT_TRUE(ret == buf);
    ASSERT_EQ(buf[0], 0);

    return true;
}

static bool test_memcpy_basic(void) {
    const char src[] = "Hello, Kernel!";
    char dst[32];
    memset(dst, 0, sizeof(dst));

    void *ret = memcpy(dst, src, sizeof(src));
    ASSERT_TRUE(ret == dst);
    ASSERT_STR_EQ(dst, "Hello, Kernel!");

    /* Zero length */
    char dummy = 'A';
    memcpy(&dummy, src, 0);
    ASSERT_EQ(dummy, 'A');

    return true;
}

static bool test_memmove_overlap(void) {
    char buf[32] = "0123456789";

    /* Overlapping: src < dst */
    memmove(buf + 2, buf, 6); /* "0101234589" */
    ASSERT_EQ(buf[0], '0');
    ASSERT_EQ(buf[1], '1');
    ASSERT_EQ(buf[2], '0');
    ASSERT_EQ(buf[3], '1');
    ASSERT_EQ(buf[4], '2');
    ASSERT_EQ(buf[5], '3');
    ASSERT_EQ(buf[6], '4');
    ASSERT_EQ(buf[7], '5');

    /* Overlapping: dst < src */
    char buf2[32] = "abcdefghij";
    memmove(buf2, buf2 + 3, 5); /* "defgh" into buf2[0..4] */
    ASSERT_EQ(buf2[0], 'd');
    ASSERT_EQ(buf2[1], 'e');
    ASSERT_EQ(buf2[2], 'f');
    ASSERT_EQ(buf2[3], 'g');
    ASSERT_EQ(buf2[4], 'h');

    /* Same source and dest */
    memmove(buf2, buf2, 10);
    ASSERT_EQ(buf2[0], 'd');

    return true;
}

static bool test_memcmp_basic(void) {
    char a[] = "AtaOS";
    char b[] = "AtaOS";
    char c[] = "AtaOx";

    ASSERT_EQ(memcmp(a, b, 5), 0);
    ASSERT_TRUE(memcmp(a, c, 5) < 0);
    ASSERT_TRUE(memcmp(c, a, 5) > 0);
    ASSERT_EQ(memcmp(a, c, 4), 0);
    ASSERT_EQ(memcmp(a, c, 0), 0);

    return true;
}

static bool test_strlen_and_strnlen(void) {
    const char str[] = "AtaOS Boot";
    ASSERT_EQ(strlen(str), 10);
    ASSERT_EQ(strlen(""), 0);

    ASSERT_EQ(strnlen(str, 5), 5);
    ASSERT_EQ(strnlen(str, 10), 10);
    ASSERT_EQ(strnlen(str, 20), 10);
    ASSERT_EQ(strnlen(str, 0), 0);

    return true;
}

static bool test_strncmp_basic(void) {
    ASSERT_EQ(strncmp("hello", "hello", 5), 0);
    ASSERT_EQ(strncmp("hello", "helloworld", 5), 0);
    ASSERT_TRUE(strncmp("hello", "world", 5) < 0);
    ASSERT_TRUE(strncmp("world", "hello", 5) > 0);
    ASSERT_EQ(strncmp("abc", "xyz", 0), 0);

    return true;
}

static bool test_strlcpy_basic(void) {
    char dst[8];
    size_t len;

    len = strlcpy(dst, "abc", sizeof(dst));
    ASSERT_EQ(len, 3);
    ASSERT_STR_EQ(dst, "abc");

    /* Truncation check */
    len = strlcpy(dst, "abcdefghijk", sizeof(dst));
    ASSERT_EQ(len, 11);
    ASSERT_EQ(strlen(dst), 7);
    ASSERT_STR_EQ(dst, "abcdefg");

    /* Zero buffer size */
    len = strlcpy(dst, "xyz", 0);
    ASSERT_EQ(len, 3);
    ASSERT_STR_EQ(dst, "abcdefg");

    return true;
}

void test_string_suite(void) {
    static const test_case_t tests[] = {
        {"memset basic and zero-length", test_memset_basic},
        {"memcpy basic", test_memcpy_basic},
        {"memmove overlapping buffers", test_memmove_overlap},
        {"memcmp comparisons", test_memcmp_basic},
        {"strlen and strnlen", test_strlen_and_strnlen},
        {"strncmp comparisons", test_strncmp_basic},
        {"strlcpy sizing and truncation", test_strlcpy_basic},
    };
    run_test_suite("kernel/lib/string.c", tests, sizeof(tests) / sizeof(tests[0]));
}
