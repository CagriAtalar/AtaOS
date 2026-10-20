/*
 * mm_internal.h - shared page-table tree walkers for kernel/mm (private).
 *
 * Visible only to kernel/mm translation units; kernel/syscall, drivers and
 * others must use kernel/vmm.h or kernel/aspace.h instead. All encodings
 * remain behind the arch hooks (include/kernel/arch.h).
 */
#ifndef ATAOS_KERNEL_MM_INTERNAL_H
#define ATAOS_KERNEL_MM_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct arch_image_ranges;

/* Table level identity for 4-level paging. */
#define MM_LEVEL_PML4 4
#define MM_LEVEL_PDPT 3
#define MM_LEVEL_PD   2
#define MM_LEVEL_PT   1

/* Kernel-half root: index in the PML4 where the shared kernel half starts. */
#define MM_PML4_KERNEL_FIRST 256U

/*
 * Canonical virtual base covered by one PML4 entry. PML4 indices 256..511
 * describe the sign-extended kernel half; indices 0..255 describe the low
 * canonical half. The result always has bits 63..48 equal to bit 47.
 */
uint64_t mm_canonical_base(unsigned pml4_index);

/*
 * Allocate and zero a page-table frame. mm_alloc_table() is for boot-time
 * paths and panics on physical-memory exhaustion. mm_alloc_table_try() is for
 * fallible mapping paths and returns 0 on exhaustion. Both bump *counter when
 * non-NULL.
 */
uint64_t mm_alloc_table(size_t *counter);
uint64_t mm_alloc_table_try(size_t *counter);

/* Release a table frame previously obtained from mm_alloc_table[_try](). */
void mm_free_table(uint64_t frame, size_t *counter);

/* Result of an attempt to install one 4 KiB leaf. */
enum mm_set_result {
    MM_SET_OK = 0,
    MM_SET_EXISTS,
    MM_SET_BLOCKED, /* a large leaf occupies the path */
    MM_SET_NOMEM,
};

/*
 * Descend the 4-level tree rooted at `root` and write `leaf_entry` at the
 * 4 KiB leaf position for `vaddr`. Missing intermediate tables are allocated
 * with mm_alloc_table_try(); newly allocated tables are unwound on failure.
 * The root table itself must already exist (kernel mappings use preseeded
 * slots). *out_pt, when non-NULL, receives the distributing PT frame on
 * MM_SET_OK only.
 */
enum mm_set_result mm_set_leaf(uint64_t root, uint64_t vaddr, uint64_t leaf_entry, bool user,
                               size_t *counter, uint64_t *out_pt);

/*
 * Translate `vaddr` through `root`. Returns true and writes the exact physical
 * address (frame/large-page base plus the in-page offset) to *paddr_out when a
 * mapping exists; false when nothing is present.
 */
bool mm_translate(uint64_t root, uint64_t vaddr, uint64_t *paddr_out);

/* Result of walking one virtual address to its leaf or first missing level. */
struct mm_query {
    bool present;
    bool leaf;
    unsigned level; /* 0 when absent, otherwise 4..1 */
    uint64_t entry;
};

/* Fill *out with the mapping state for `vaddr` under `root`. */
void mm_query_mapping(uint64_t root, uint64_t vaddr, struct mm_query *out);

/*
 * Clear the 4 KiB leaf for `vaddr`. When `allow_free` is set, subtrees that
 * become empty are released (but never the root itself); *counter shrinks
 * accordingly (may be NULL). Returns false when the leaf was not present or a
 * large leaf blocks the position.
 */
bool mm_clear_leaf(uint64_t root, uint64_t vaddr, bool allow_free, size_t *counter);

/*
 * Release every table frame in the subtree rooted at `table` (whose entries
 * sit at `level`) and then the table itself. Present leaf entries are left
 * untouched: leaf data frames are caller-owned. Used by aspace_destroy() for
 * the private user half only.
 */
void mm_free_subtree(uint64_t table, unsigned level, size_t *counter);

/*
 * True when a large leaf at `level` (2 or 3) covering `vaddr` overlaps any
 * part of the kernel image. Re-protecting such a leaf would either make image
 * data executable or make text non-executable, so boot must panic instead.
 */
bool mm_large_leaf_intersects_image(uint64_t vaddr, unsigned level,
                                    const struct arch_image_ranges *ranges);

/* Expected permission state of a pre-flight mapping check. */
enum mm_expect {
    MM_EXPECT_RX,     /* present, executable, not writable */
    MM_EXPECT_RW_NX,  /* present, writable, not executable */
    MM_EXPECT_R_NX,   /* present, not executable; write may be either */
    MM_EXPECT_UNMAPPED,
};

struct mm_preflight_spec {
    uint64_t vaddr;
    enum mm_expect expect;
    const char *name;
};

struct mm_preflight_failure {
    const char *name;
    uint64_t vaddr;
    enum mm_expect expect;
    struct mm_query actual;
};

/*
 * Check a list of critical mappings in `root`. Returns true when every
 * specification holds; otherwise fills *failure (when non-NULL) with the first
 * mismatch and returns false. No allocation, no CPU access beyond the table
 * hooks: safe while the old CR3 is still active.
 */
bool mm_check_mappings(uint64_t root, const struct mm_preflight_spec *specs, size_t count,
                       struct mm_preflight_failure *failure);

#endif /* ATAOS_KERNEL_MM_INTERNAL_H */
