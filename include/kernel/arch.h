/*
 * arch.h - the narrow interface between architecture-independent kernel code
 * and the architecture layer (arch/x86_64/).
 *
 * Architecture-independent code may call these functions; it must not use
 * inline assembly, port I/O or control registers directly.
 *
 * Status: M4. The interface contains the early-console, halt and test-exit
 * hooks plus the page-table/CPU-memory boundary used by kernel/mm. It grows
 * milestone by milestone (see docs/ARCHITECTURE.md).
 */
#ifndef ATAOS_KERNEL_ARCH_H
#define ATAOS_KERNEL_ARCH_H

#include <stdbool.h>
#include <stdint.h>

#include "kernel/compiler.h"

/*
 * Initialise the early serial console. Safe to call before anything else; uses
 * no memory allocation. Best-effort: there is no error return because the
 * panic path must work even if the console is absent.
 */
void arch_early_console_init(void);

/*
 * Initialise the custom x86_64 Global Descriptor Table (GDT) and Task State
 * Segment (TSS). Loads 64-bit kernel and user segment descriptors, points RSP0
 * to the bootstrap stack, populates IST stacks for #DF, NMI, and #MC, and
 * loads the Task Register (TR). Must be called with interrupts disabled.
 */
void arch_gdt_init(void);

/*
 * Initialise the minimal Interrupt Descriptor Table (IDT).
 * Sets up 256 present 64-bit interrupt gates pointing to an emergency exception
 * stub, binds critical exception vectors to dedicated IST stacks (NMI -> IST2,
 * #DF -> IST1, #MC -> IST3), and loads IDTR via `lidt`.
 * Interrupts remain disabled.
 */
void arch_idt_init(void);

/* Write one character to the early console. Callable from any context. */
void arch_early_console_putc(char c);

/* Disable maskable interrupts on the current CPU. */
void arch_irq_disable(void);

/* Disable interrupts and halt the CPU forever. Never returns. */
ATAOS_NORETURN void arch_halt_forever(void);

/*
 * Kernel image section ranges (linker-provided; see arch/x86_64/linker.ld).
 * Used by kernel/mm to re-protect the kernel image in its own page tables
 * (M4). All addresses are kernel virtual; ranges are page-aligned.
 */
struct arch_image_ranges {
    uintptr_t text_start;
    uintptr_t text_end; /* exclusive; executable */
    uintptr_t rodata_start;
    uintptr_t rodata_end; /* exclusive; read-only */
    uintptr_t data_start;
    uintptr_t data_end; /* exclusive; data/bss writable */
};

/* Fill `out` with the kernel image section ranges (never fails). */
void arch_image_ranges(struct arch_image_ranges *out);

/*
 * M4: page-table and TLB hooks. The architecture layer owns every page-table
 * bit encoding; kernel/mm drives the page-table tree through these helpers
 * only. 4-level paging facts (entry count, level shifts) are exposed as
 * constants so kernel/mm can compute indices without arch-private headers.
 */
#define ARCH_MM4_TABLE_ENTRIES 512ULL
#define ARCH_MM4_SHIFT_L1      12ULL
#define ARCH_MM4_SHIFT_L2      21ULL
#define ARCH_MM4_SHIFT_L3      30ULL
#define ARCH_MM4_SHIFT_L4      39ULL

/*
 * Decoded permission state of a page-table entry, as seen by kernel/mm
 * policy code. `exec` is only meaningful for present entries and when
 * EFER.NXE is enabled (guaranteed by arch_mm_cpu_memory_init()).
 */
struct arch_mm_entry_flags {
    bool present;
    bool write;
    bool user;
    bool exec;
    bool large; /* PS bit: leaf page at this level (2 MiB / 1 GiB) */
};

/*
 * CPU-level paging setup (M4): CR0.WP = 1, EFER.NXE = 1, CR4.SMEP/SMAP when
 * CPUID reports them. Must be called with interrupts disabled.
 */
void arch_mm_cpu_memory_init(void);

/* Register the HHDM base the arch layer uses for phys<->virt table access. */
void arch_mm_set_hhdm_base(uint64_t hhdm_base);

/* Read/write one 64-bit entry of a table page given its physical address. */
uint64_t arch_mm_table_read(uint64_t table_phys, size_t index);
void arch_mm_table_write(uint64_t table_phys, size_t index, uint64_t entry);
void arch_mm_table_zero(uint64_t table_phys);

/* Entry builders: kernel/mm never hands out encoded values itself. */
uint64_t arch_mm_entry_table(uint64_t table_phys, bool user);
uint64_t arch_mm_entry_leaf(uint64_t phys, bool write, bool exec, bool user, bool large);

/*
 * Build a leaf entry from an existing one, preserving the physical frame
 * and page-size (PS) bits, but setting permissions per the arguments.
 * Executable means NX clear. Used by vmm to re-protect copied bootloader
 * leaves without naming encoding bits.
 */
uint64_t arch_mm_leaf_reprotect(uint64_t entry, bool write, bool exec, bool user);

/*
 * Decode one entry at paging `level` (4=PML4, 3=PDPT, 2=PD, 1=PT) into
 * arch-independent flags. The level is required because bit 7 is PS in a
 * PDE/PDPTE but PAT in a PTE, and bit 12 is PAT in 2 MiB/1 GiB leaves.
 */
void arch_mm_entry_flags(uint64_t entry, unsigned level, struct arch_mm_entry_flags *out);

/* True when the present bit is set. */
bool arch_mm_entry_present(uint64_t entry);

/*
 * Physical frame address stored in a table entry or 4 KiB leaf. Bit 12 is
 * part of the address here; use arch_mm_entry_leaf_phys() for large leaves.
 */
uint64_t arch_mm_entry_phys(uint64_t entry);

/*
 * Physical base address of a leaf at `level`: bit 12 is the PAT bit for 2 MiB
 * and 1 GiB leaves and must not leak into the address. `level` must be 1..3.
 */
uint64_t arch_mm_entry_leaf_phys(uint64_t entry, unsigned level);

/* True when the entry points to the next level (present, not a large leaf). */
bool arch_mm_entry_is_table(uint64_t entry, unsigned level);

/* CR3 and TLB control (current CPU). */
uint64_t arch_mm_read_cr3(void);
void arch_mm_set_cr3(uint64_t pml4_phys);
void arch_mm_invlpg(uint64_t vaddr);

/*
 * Boot pre-flight information (M4). kernel/mm checks that critical code,
 * stacks and descriptor tables are mapped correctly in the new tables before
 * loading CR3. These hooks expose only addresses and sizes; all page-table
 * interpretation remains in kernel/mm.
 */
struct arch_mm_kernel_tables {
    uintptr_t gdt_base;
    size_t gdt_size;
    uintptr_t idt_base;
    size_t idt_size;
    uintptr_t tss_base;
    size_t tss_size;
    uintptr_t ist_top[3]; /* #DF, NMI, #MC (one past the last byte) */
};

void arch_mm_kernel_tables(struct arch_mm_kernel_tables *out);
uintptr_t arch_mm_read_rsp(void);
void arch_mm_boot_stack_bounds(uintptr_t *low, uintptr_t *high);
uintptr_t arch_mm_exception_dispatch_addr(void);
uintptr_t arch_mm_exception_stub_addr(unsigned vector);

#ifdef ATAOS_TEST_BUILD
/*
 * Test-only exit codes written to the QEMU isa-debug-exit device (I/O port
 * 0xf4). QEMU turns a written value v into process exit status (v << 1) | 1.
 *   ARCH_TEST_EXIT_PASS (0x10) -> exit status 33
 *   ARCH_TEST_EXIT_FAIL (0x11) -> exit status 35
 * These symbols exist ONLY when the kernel is built with -DATAOS_TEST_BUILD.
 */
#define ARCH_TEST_EXIT_PASS 0x10
#define ARCH_TEST_EXIT_FAIL 0x11

/*
 * Request emulator exit with the given code. If no debug-exit device is
 * present the call returns and the caller must halt.
 */
void arch_test_exit(int code);

/*
 * Deliberately trigger exceptions for testing CPU exception handling (M2):
 *   - #DE: Divide Error (division by zero)
 *   - #UD: Invalid Opcode (via `ud2`)
 *   - #GP: General Protection Fault (non-canonical memory access)
 *   - #PF: Page Fault (unmapped page access)
 *   - #DF: Double Fault (stack exhaustion/corruption switching to IST1)
 */
void arch_test_fault_de(void);
void arch_test_fault_ud(void);
void arch_test_fault_gp(void);
void arch_test_fault_pf(void);
void arch_test_fault_df(void);
#endif

#endif /* ATAOS_KERNEL_ARCH_H */
