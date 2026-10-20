#include "test_framework.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

int g_tests_run = 0;
int g_tests_passed = 0;
int g_tests_failed = 0;

void panic_at(const char *file, int line, const char *fmt, ...) {
    fprintf(stderr, "HOST PANIC at %s:%d: ", file, line);
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
    abort();
}

void run_test_suite(const char *suite_name, const test_case_t *tests, size_t count) {
    printf("[SUITE] %s (%zu tests)\n", suite_name, count);
    for (size_t i = 0; i < count; i++) {
        g_tests_run++;
        printf("  - %s ... ", tests[i].name);
        fflush(stdout);
        if (tests[i].fn()) {
            g_tests_passed++;
            printf("PASS\n");
        } else {
            g_tests_failed++;
            printf("FAIL\n");
        }
    }
}

extern void test_string_suite(void);
extern void test_fmt_suite(void);
extern void test_bits_suite(void);
extern void test_bitmap_suite(void);
extern void test_heap_suite(void);
extern void test_mm_suite(void);
extern void test_pf_suite(void);

int main(void) {
    printf("=== AtaOS Host Unit Test Runner ===\n\n");

    test_string_suite();
    test_fmt_suite();
    test_bits_suite();
    test_bitmap_suite();
    test_heap_suite();
    test_mm_suite();
    test_pf_suite();

    printf("\n=== Summary ===\n");
    printf("Total:  %d\n", g_tests_run);
    printf("Passed: %d\n", g_tests_passed);
    printf("Failed: %d\n", g_tests_failed);

    if (g_tests_failed > 0) {
        printf("RESULT: FAILED (%d failures)\n", g_tests_failed);
        return 1;
    }

    printf("RESULT: ALL TESTS PASSED\n");
    return 0;
}
