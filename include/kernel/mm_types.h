/*
 * mm_types.h - common memory-management constants.
 *
 * Kept separate from pmm.h so page-size users do not depend on the physical
 * frame allocator interface. All sizes are in bytes unless stated otherwise.
 */
#ifndef ATAOS_KERNEL_MM_TYPES_H
#define ATAOS_KERNEL_MM_TYPES_H

#include <stdint.h>

/* Base page size of the architecture (4 KiB, x86_64 4-level paging). */
#define PAGE_SIZE  4096ULL
#define PAGE_SHIFT 12U

/* Mask of the in-page offset for a base page. */
#define PAGE_OFFSET_MASK (PAGE_SIZE - 1ULL)

#endif /* ATAOS_KERNEL_MM_TYPES_H */
