/*
 * paging.h - x86_64 page-table bit encodings (arch-internal, M4).
 *
 * This header owns every page-table bit definition and CPU control bit used
 * by the memory architecture. Function prototypes for arch_mm_* live in
 * include/kernel/arch.h (the only interface kernel/mm may call through);
 * this header stays architecture-private and reaches arch sources only.
 *
 * Format: 4-level x86_64 paging, 4 KiB and 2 MiB / 1 GiB large pages,
 * physical addresses in bits 12..51 of an entry, NX in bit 63.
 */
#ifndef ATAOS_ARCH_X86_64_PAGING_H
#define ATAOS_ARCH_X86_64_PAGING_H

/* Number of entries in one 4 KiB page-table page. */
#define MM_TABLE_ENTRIES 512ULL

/* Entry flag bits. */
#define MM_PTE_PRESENT (1ULL << 0)
#define MM_PTE_WRITE   (1ULL << 1)
#define MM_PTE_USER    (1ULL << 2)
#define MM_PTE_PWT     (1ULL << 3)
#define MM_PTE_PCD     (1ULL << 4)
#define MM_PTE_ACCESSED (1ULL << 5)
#define MM_PTE_DIRTY    (1ULL << 6)
#define MM_PTE_PAT     (1ULL << 7) /* 4 KiB PTE; PAT for large leaves is bit 12 */
#define MM_PTE_LARGE   (1ULL << 7) /* PDE: 2 MiB page, PDPTE: 1 GiB page */
#define MM_PTE_G       (1ULL << 8)
#define MM_PTE_NX      (1ULL << 63)

/* Physical address bits stored in a table entry: 12..51. */
#define MM_ADDR_MASK 0x000ffffffffff000ULL

/* CR0.WP (bit 16): supervisor write protection. */
#define MM_CR0_WP (1ULL << 16)

/* IA32_EFER MSR (0xC0000080) and EFER.NXE (bit 11). */
#define IA32_EFER_MSR 0xC0000080U
#define MM_EFER_NXE   (1ULL << 11)

/* CR4.SMEP (bit 20) / CR4.SMAP (bit 21). */
#define MM_CR4_SMEP (1ULL << 20)
#define MM_CR4_SMAP (1ULL << 21)

/* CPUID leaf 7 (%ecx=0) EBX feature bits. */
#define MM_CPUID7_EBX_SMEP (1U << 7)
#define MM_CPUID7_EBX_SMAP (1U << 20)

/*
 * Generic 4-level paging indices are derived by kernel/mm from these public
 * arch.h constants (see include/kernel/arch.h); no private shift constants
 * are needed here.
 */

#endif /* ATAOS_ARCH_X86_64_PAGING_H */
