/*
 * page_fault.c - decode x86_64 #PF error codes for diagnostics.
 *
 * This file is deliberately free of inline assembly and CPU access so the
 * exact bit mapping can be unit-tested on the host.
 */
#include "arch/page_fault.h"

struct x86_page_fault x86_decode_page_fault(uint64_t error_code)
{
    struct x86_page_fault pf;

    pf.present = (error_code & X86_PF_PRESENT) != 0;
    pf.write = (error_code & X86_PF_WRITE) != 0;
    pf.user = (error_code & X86_PF_USER) != 0;
    pf.reserved = (error_code & X86_PF_RESERVED) != 0;
    pf.instruction_fetch = (error_code & X86_PF_FETCH) != 0;
    pf.protection_key = (error_code & X86_PF_PK) != 0;
    pf.shadow_stack = (error_code & X86_PF_SGX) != 0;
    return pf;
}

const char *x86_pf_access(const struct x86_page_fault *pf)
{
    if (pf->instruction_fetch)
        return "fetch";
    return pf->write ? "write" : "read";
}

const char *x86_pf_origin(const struct x86_page_fault *pf)
{
    return pf->user ? "user" : "kernel";
}

const char *x86_pf_cause(const struct x86_page_fault *pf)
{
    return pf->present ? "protection" : "not-present";
}

const char *x86_pf_fetch_kind(const struct x86_page_fault *pf)
{
    return pf->instruction_fetch ? "instruction" : "data";
}
