#ifndef ATAOS_TEST_FRAMEWORK_H
#define ATAOS_TEST_FRAMEWORK_H

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>

extern int g_tests_run;
extern int g_tests_passed;
extern int g_tests_failed;

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("  [FAIL] %s:%d: %s\n", __FILE__, __LINE__, msg); \
        return false; \
    } \
} while (0)

#define ASSERT_TRUE(cond) TEST_ASSERT((cond), #cond " is false, expected true")
#define ASSERT_FALSE(cond) TEST_ASSERT(!(cond), #cond " is true, expected false")

#define ASSERT_EQ(a, b) do { \
    int64_t _val_a = (int64_t)(a); \
    int64_t _val_b = (int64_t)(b); \
    if (_val_a != _val_b) { \
        printf("  [FAIL] %s:%d: %s == %s (0x%lx != 0x%lx)\n", \
               __FILE__, __LINE__, #a, #b, (unsigned long)_val_a, (unsigned long)_val_b); \
        return false; \
    } \
} while (0)

#define ASSERT_STR_EQ(a, b) do { \
    const char *_sa = (const char *)(a); \
    const char *_sb = (const char *)(b); \
    if (strcmp(_sa, _sb) != 0) { \
        printf("  [FAIL] %s:%d: \"%s\" != \"%s\"\n", __FILE__, __LINE__, _sa, _sb); \
        return false; \
    } \
} while (0)

#define ASSERT_MEM_EQ(a, b, n) do { \
    const void *_ma = (const void *)(a); \
    const void *_mb = (const void *)(b); \
    size_t _sz = (size_t)(n); \
    if (memcmp(_ma, _mb, _sz) != 0) { \
        printf("  [FAIL] %s:%d: memcmp mismatch across %zu bytes\n", __FILE__, __LINE__, _sz); \
        return false; \
    } \
} while (0)

typedef bool (*test_fn_t)(void);

typedef struct {
    const char *name;
    test_fn_t fn;
} test_case_t;

void run_test_suite(const char *suite_name, const test_case_t *tests, size_t count);

#endif /* ATAOS_TEST_FRAMEWORK_H */
