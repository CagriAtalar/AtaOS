/*
 * page_fault.h - x86_64 #PF error-code decoding.
 *
 * Pure, host-testable decoding. The exception dispatcher is responsible for
 * reading CR2 and deciding whether the fault is fatal in the current mode.
 */
#ifndef ATAOS_ARCH_X86_64_PAGE_FAULT_H
#define ATAOS_ARCH_X86_64_PAGE_FAULT_H

#include <stdbool.h>
#include <stdint.h>

/* Bits defined by the x86_64 #PF error code (SDM Vol. 3, 6.15). */
#define X86_PF_PRESENT   (1ULL << 0)
#define X86_PF_WRITE     (1ULL << 1)
#define X86_PF_USER      (1ULL << 2)
#define X86_PF_RESERVED  (1ULL << 3)
#define X86_PF_FETCH     (1ULL << 4)
#define X86_PF_PK        (1ULL << 5)
#define X86_PF_SGX       (1ULL << 6)

struct x86_page_fault {
    bool present;           /* 1 = protection violation, 0 = not-present */
    bool write;             /* 1 = write access, 0 = read/fetch */
    bool user;              /* 1 = CPL3/user access */
    bool reserved;          /* reserved-bit violation in a paging structure */
    bool instruction_fetch; /* 1 = instruction fetch (NX/execute violation) */
    bool protection_key;    /* PK protection violation */
    bool shadow_stack;      /* CET shadow-stack violation */
};

struct x86_page_fault x86_decode_page_fault(uint64_t error_code);

const char *x86_pf_access(const struct x86_page_fault *pf);
const char *x86_pf_origin(const struct x86_page_fault *pf);
const char *x86_pf_cause(const struct x86_page_fault *pf);
const char *x86_pf_fetch_kind(const struct x86_page_fault *pf);

#endif /* ATAOS_ARCH_X86_64_PAGE_FAULT_H */
