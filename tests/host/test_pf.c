/*
 * test_pf.c - host tests for x86_64 #PF error-code decoding.
 */
#include "test_framework.h"

#include <arch/page_fault.h>

static bool test_decode_all_bits(void)
{
    struct x86_page_fault pf = x86_decode_page_fault(
        X86_PF_PRESENT | X86_PF_WRITE | X86_PF_USER | X86_PF_RESERVED |
        X86_PF_FETCH | X86_PF_PK | X86_PF_SGX);

    ASSERT_TRUE(pf.present);
    ASSERT_TRUE(pf.write);
    ASSERT_TRUE(pf.user);
    ASSERT_TRUE(pf.reserved);
    ASSERT_TRUE(pf.instruction_fetch);
    ASSERT_TRUE(pf.protection_key);
    ASSERT_TRUE(pf.shadow_stack);
    ASSERT_STR_EQ(x86_pf_access(&pf), "fetch");
    ASSERT_STR_EQ(x86_pf_origin(&pf), "user");
    ASSERT_STR_EQ(x86_pf_cause(&pf), "protection");
    ASSERT_STR_EQ(x86_pf_fetch_kind(&pf), "instruction");
    return true;
}

static bool test_decode_supervisor_demand_write(void)
{
    struct x86_page_fault pf = x86_decode_page_fault(X86_PF_WRITE);

    ASSERT_FALSE(pf.present);
    ASSERT_TRUE(pf.write);
    ASSERT_FALSE(pf.user);
    ASSERT_FALSE(pf.reserved);
    ASSERT_FALSE(pf.instruction_fetch);
    ASSERT_STR_EQ(x86_pf_access(&pf), "write");
    ASSERT_STR_EQ(x86_pf_origin(&pf), "kernel");
    ASSERT_STR_EQ(x86_pf_cause(&pf), "not-present");
    ASSERT_STR_EQ(x86_pf_fetch_kind(&pf), "data");
    return true;
}

static bool test_decode_protection_read(void)
{
    struct x86_page_fault pf = x86_decode_page_fault(X86_PF_PRESENT);

    ASSERT_TRUE(pf.present);
    ASSERT_FALSE(pf.write);
    ASSERT_STR_EQ(x86_pf_access(&pf), "read");
    ASSERT_STR_EQ(x86_pf_cause(&pf), "protection");
    return true;
}

void test_pf_suite(void)
{
    static const test_case_t tests[] = {
        {"decode all #PF bits", test_decode_all_bits},
        {"decode supervisor demand write", test_decode_supervisor_demand_write},
        {"decode protection read", test_decode_protection_read},
    };

    run_test_suite("arch/x86_64/interrupt/page_fault.c", tests,
                   sizeof(tests) / sizeof(tests[0]));
}
