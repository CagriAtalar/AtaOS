/*
 * aspace_internal.h - kernel/mm-private helpers for address-space tests.
 */
#ifndef ATAOS_KERNEL_MM_ASPACE_INTERNAL_H
#define ATAOS_KERNEL_MM_ASPACE_INTERNAL_H

#include <stdint.h>

#include "kernel/aspace.h"

/* Same as aspace_create(), but with an explicit master PML4 (host tests). */
int aspace_create_from(uint64_t kernel_pml4, struct aspace **out);

#endif /* ATAOS_KERNEL_MM_ASPACE_INTERNAL_H */
