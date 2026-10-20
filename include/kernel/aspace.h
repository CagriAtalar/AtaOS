/*
 * aspace.h - per-process address-space lifecycle (M4).
 *
 * An address space owns the low canonical half (0..VMM_USER_TOP). Its PML4
 * shares the master kernel half with every other space. M4 creates and
 * destroys spaces and provides the mapping calls M5 will use; leaf data
 * frames remain caller-owned (see G4 and docs/MEMORY_LAYOUT.md §6).
 */
#ifndef ATAOS_KERNEL_ASPACE_H
#define ATAOS_KERNEL_ASPACE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "kernel/vmm.h"

struct aspace;

/*
 * Register the master kernel PML4 whose upper 256 entries every new space
 * shares. Called once by vmm_init() after the master tables are audited.
 */
void aspace_init(uint64_t kernel_pml4);

/*
 * Allocate a PML4, copy PML4 entries 256..511 from the registered master
 * space, and zero the private user half. Returns 0 or -ENOMEM.
 */
int aspace_create(struct aspace **out);

/*
 * Release all private user-half table frames and the PML4. Shared kernel
 * entries are never freed. Leaf data frames are caller-owned and are not
 * released by this function.
 */
void aspace_destroy(struct aspace *as);

uint64_t aspace_pml4(const struct aspace *as);
size_t aspace_table_frames(const struct aspace *as);

/*
 * Map npages 4 KiB pages at `vaddr` from `paddr` with VMM_USER required.
 * Rejects WRITE|EXEC, kernel addresses, unaligned/overflowing ranges and
 * overlaps (-EEXIST). Newly allocated tables are unwound on failure.
 */
int aspace_map(struct aspace *as, uintptr_t vaddr, uint64_t paddr, size_t npages,
               unsigned flags);

/* Remove user mappings. Returns -ENOENT when any page was absent. */
int aspace_unmap(struct aspace *as, uintptr_t vaddr, size_t npages);

bool aspace_resolve(const struct aspace *as, uintptr_t vaddr, uint64_t *paddr_out);

/*
 * Boot-time in-kernel suite: create a space, map/resolve a user page,
 * exercise W^X/overlap/ENOENT rejection, destroy the space and verify PMM
 * counters return to their pre-test values. Runs after heap_init().
 */
void aspace_self_test(void);

#endif /* ATAOS_KERNEL_ASPACE_H */
