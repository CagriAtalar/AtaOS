/*
 * fake_pmm.h - deterministic host-test frame allocator.
 */
#ifndef ATAOS_TESTS_HOST_FAKE_PMM_H
#define ATAOS_TESTS_HOST_FAKE_PMM_H

#include <stddef.h>

/* Reset the fake allocator: all frames free except frame zero. */
void fake_pmm_reset(void);

/*
 * Allow exactly `successes` successful allocations before returning
 * PADDR_INVALID. SIZE_MAX (the reset default) means never fail.
 */
void fake_pmm_fail_after(size_t successes);

/* Number of frames currently allocated through the fake PMM. */
size_t fake_pmm_allocated(void);

#endif /* ATAOS_TESTS_HOST_FAKE_PMM_H */
