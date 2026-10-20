/*
 * fake_arch.h - host-test replacement for the x86_64 page-table memory layer.
 *
 * The fake owns a small physical-memory arena. Page-table frames handed out by
 * fake_pmm.c live in that arena, so the real tree walkers can read and write
 * them without executing privileged instructions.
 */
#ifndef ATAOS_TESTS_HOST_FAKE_ARCH_H
#define ATAOS_TESTS_HOST_FAKE_ARCH_H

#include <stddef.h>
#include <stdint.h>

/* Physical base and size of the fake memory arena. */
#define FAKE_PHYS_BASE  0x00100000ULL
#define FAKE_FRAME_SIZE 4096ULL
#define FAKE_FRAME_COUNT 1024U

/* Translate a physical address inside the fake arena to a host pointer. */
uint8_t *fake_phys_ptr(uint64_t paddr);

/* Forget any configured image ranges or synthetic critical addresses. */
void fake_arch_reset(void);

#endif /* ATAOS_TESTS_HOST_FAKE_ARCH_H */
