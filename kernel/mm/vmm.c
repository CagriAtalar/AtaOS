/*
 * vmm.c - virtual memory mapping policy (M4).
 *
 * Builds the master kernel page tables by deep-copying the bootloader's
 * kernel half into kernel-owned frames and re-protecting every leaf:
 * kernel text keeps executable+read-only and every other kernel-half page
 * (HHDM, rodata, data/bss, firmware mappings) becomes non-executable, so
 * no leaf is both writable and executable and none is user-accessible.
 * See ADR-0007 and docs/MEMORY_LAYOUT.md §5/§6.
 *
 * All page-table encodings are performed by the arch layer through
 * include/kernel/arch.h; this file only decides where, what and when.
 * Tree walks are shared with aspace.c via mm_internal.h (kernel/mm private).
 */
#include "kernel/vmm.h"
#include "kernel/arch.h"
#include "kernel/bits.h"
#include "kernel/bootinfo.h"
#include "kernel/errno.h"
#include "kernel/log.h"
#include "kernel/panic.h"
#include "kernel/pmm.h"

#include "mm_internal.h"
#include "vmm_internal.h"

static uint64_t s_master_pml4 = 0;
static size_t s_master_table_frames = 0; /* frames currently owned by master space */
static bool s_vmm_initialised = false;

/* Shift of entries sitting at `level` (PML4=4 ... PT=1). */
static unsigned shift_at_level(unsigned level)
{
    switch (level) {
    case MM_LEVEL_PML4:
        return (unsigned)ARCH_MM4_SHIFT_L4;
    case MM_LEVEL_PDPT:
        return (unsigned)ARCH_MM4_SHIFT_L3;
    case MM_LEVEL_PD:
        return (unsigned)ARCH_MM4_SHIFT_L2;
    case MM_LEVEL_PT:
        return (unsigned)ARCH_MM4_SHIFT_L1;
    default:
        PANIC("vmm: invalid table level %u", level);
    }
}

/* Refuse an HHDM that would grow into the dedicated kernel heap slot. */
static void check_hhdm_bounds(const struct bootinfo *bi)
{
    uint64_t mapped_end = 0;
    for (size_t i = 0; i < bi->memmap_count; i++) {
        bool mapped_type = bi->memmap[i].type == BOOT_MEM_USABLE ||
                           bi->memmap[i].type == BOOT_MEM_BOOTLOADER_RECLAIMABLE ||
                           bi->memmap[i].type == BOOT_MEM_KERNEL_AND_MODULES ||
                           bi->memmap[i].type == BOOT_MEM_FRAMEBUFFER ||
                           bi->memmap[i].type == BOOT_MEM_RESERVED_MAPPED ||
                           bi->memmap[i].type == BOOT_MEM_ACPI_RECLAIMABLE ||
                           bi->memmap[i].type == BOOT_MEM_ACPI_NVS;
        if (!mapped_type)
            continue;
        uint64_t end;
        if (add_overflows_u64(bi->memmap[i].base, bi->memmap[i].length, &end))
            PANIC("vmm: memory region overflows at base 0x%lx",
                  (unsigned long)bi->memmap[i].base);
        if (end > mapped_end)
            mapped_end = end;
    }

    uint64_t hhdm_top;
    if (add_overflows_u64(bi->hhdm_offset, mapped_end, &hhdm_top) ||
        hhdm_top > VMM_KERN_HEAP_BASE)
        PANIC("vmm: HHDM would overlap the kernel heap region (top 0x%lx)",
              (unsigned long)hhdm_top);
}

static const char *intersecting_section(uint64_t vaddr, unsigned level,
                                        const struct arch_image_ranges *r)
{
    uint64_t size = 1ULL << shift_at_level(level);
    uint64_t end = vaddr + size;
    if (end < vaddr)
        end = UINT64_MAX;

    if (vaddr < (uint64_t)r->text_end && end > (uint64_t)r->text_start)
        return ".text";
    if (vaddr < (uint64_t)r->rodata_end && end > (uint64_t)r->rodata_start)
        return ".rodata";
    if (vaddr < (uint64_t)r->data_end && end > (uint64_t)r->data_start)
        return ".data/.bss";
    return "kernel image";
}

/*
 * Boot-time leaf re-protection for a copied kernel-half leaf covering
 * [page_vaddr, page_vaddr + page_size):
 *   - kernel text: executable + read-only (fixes any laxer bootloader bits),
 *   - rodata: read-only, non-executable,
 *   - data/bss: writable, non-executable,
 *   - everything else (HHDM, firmware regions): write attribute kept,
 *     execution forbidden.
 * The user bit is always cleared for the kernel half. A large leaf that
 * intersects the kernel image is a hard boot error: re-protecting part of the
 * image through a shared large page would make either text or data wrong.
 */
static uint64_t fix_leaf(uint64_t entry, uint64_t page_vaddr, unsigned level)
{
    struct arch_image_ranges r;
    arch_image_ranges(&r);

    struct arch_mm_entry_flags f;
    arch_mm_entry_flags(entry, level, &f);

    if (f.large && mm_large_leaf_intersects_image(page_vaddr, level, &r)) {
        uint64_t size = 1ULL << shift_at_level(level);
        uint64_t end = page_vaddr + size;
        if (end < page_vaddr)
            end = UINT64_MAX;
        PANIC("vmm: level-%u large leaf 0x%lx-0x%lx intersects kernel %s",
              level, (unsigned long)page_vaddr, (unsigned long)(end - 1),
              intersecting_section(page_vaddr, level, &r));
    }

    bool in_text = !f.large && page_vaddr >= (uint64_t)r.text_start &&
                   page_vaddr < (uint64_t)r.text_end;
    bool in_rodata = !f.large && page_vaddr >= (uint64_t)r.rodata_start &&
                     page_vaddr < (uint64_t)r.rodata_end;
    bool in_data = !f.large && page_vaddr >= (uint64_t)r.data_start &&
                   page_vaddr < (uint64_t)r.data_end;

    bool write;
    bool exec;
    if (f.large) {
        write = f.write;
        exec = false;
    } else if (in_text) {
        write = false;
        exec = true;
    } else if (in_rodata) {
        write = false;
        exec = false;
    } else if (in_data) {
        write = true;
        exec = false;
    } else {
        write = f.write;
        exec = false;
    }

    return arch_mm_leaf_reprotect(entry, write, exec, false);
}

/*
 * Deep-copy the bootloader page-table subtree headed by `src_table_phys`.
 * Its entries sit at `level` (PDPT=3 down to PT=1) and cover the range
 * with base `base_vaddr`. Large-page entries (PS bit) and PT leaves are
 * re-emitted through fix_leaf() without allocating frames; table entries
 * allocate kernel-owned frames. Returns the new table's physical address.
 */
static uint64_t copy_table(uint64_t src_table_phys, unsigned level, uint64_t base_vaddr)
{
    ASSERT_ALWAYS(level >= MM_LEVEL_PT && level <= MM_LEVEL_PDPT);

    uint64_t dst = mm_alloc_table(&s_master_table_frames);

    for (unsigned i = 0; i < ARCH_MM4_TABLE_ENTRIES; i++) {
        uint64_t entry = arch_mm_table_read(src_table_phys, i);
        struct arch_mm_entry_flags f;
        arch_mm_entry_flags(entry, level, &f);
        if (!f.present)
            continue;

        uint64_t child_vaddr = base_vaddr + ((uint64_t)i << shift_at_level(level));

        if (f.large || level == MM_LEVEL_PT) {
            arch_mm_table_write(dst, i, fix_leaf(entry, child_vaddr, level));
            continue;
        }

        uint64_t new_child = copy_table(arch_mm_entry_phys(entry), level - 1, child_vaddr);
        arch_mm_table_write(dst, i, arch_mm_entry_table(new_child, false));
    }

    return dst;
}

/*
 * Pre-populate the full PDPT/PD/PT chain of a planned kernel region's first
 * 2 MiB (ADR-0007: kernel top-level slots that will ever hold mappings
 * exist before any address space does; mappings beyond the first 2 MiB must
 * be created before the first address space exists — documented in vmm.h).
 */
static void preseed_region_slot(uint64_t region_base, const char *name)
{
    unsigned l4 = (unsigned)((region_base >> ARCH_MM4_SHIFT_L4) & (ARCH_MM4_TABLE_ENTRIES - 1));
    uint64_t pml4e = arch_mm_table_read(s_master_pml4, l4);
    uint64_t pdpt;
    if (!arch_mm_entry_present(pml4e)) {
        pdpt = mm_alloc_table(&s_master_table_frames);
        arch_mm_table_write(s_master_pml4, l4, arch_mm_entry_table(pdpt, false));
    } else {
        ASSERT_ALWAYS(arch_mm_entry_is_table(pml4e, MM_LEVEL_PML4));
        pdpt = arch_mm_entry_phys(pml4e);
    }

    unsigned l3 = (unsigned)((region_base >> ARCH_MM4_SHIFT_L3) & (ARCH_MM4_TABLE_ENTRIES - 1));
    uint64_t pdpte = arch_mm_table_read(pdpt, l3);
    uint64_t pd;
    if (!arch_mm_entry_present(pdpte)) {
        pd = mm_alloc_table(&s_master_table_frames);
        arch_mm_table_write(pdpt, l3, arch_mm_entry_table(pd, false));
    } else {
        ASSERT_ALWAYS(arch_mm_entry_is_table(pdpte, MM_LEVEL_PDPT));
        pd = arch_mm_entry_phys(pdpte);
    }

    unsigned l2 = (unsigned)((region_base >> ARCH_MM4_SHIFT_L2) & (ARCH_MM4_TABLE_ENTRIES - 1));
    uint64_t pde = arch_mm_table_read(pd, l2);
    uint64_t pt;
    if (!arch_mm_entry_present(pde)) {
        pt = mm_alloc_table(&s_master_table_frames);
        arch_mm_table_write(pd, l2, arch_mm_entry_table(pt, false));
    } else {
        ASSERT_ALWAYS(arch_mm_entry_is_table(pde, MM_LEVEL_PD));
        pt = arch_mm_entry_phys(pde);
    }

    log_info("vmm: preseeded %s top-level slot (PML4 index %u)", name, l4);
}

struct audit_counts {
    size_t text;
    size_t rodata;
    size_t data;
    size_t other;
    size_t exec;
};

static void audit_leaf(uint64_t entry, unsigned level, uint64_t vaddr,
                       const struct arch_image_ranges *r, struct audit_counts *counts)
{
    struct arch_mm_entry_flags f;
    arch_mm_entry_flags(entry, level, &f);

    ASSERT_ALWAYS(f.present);
    ASSERT_ALWAYS(!f.user);

    if (f.large) {
        if (mm_large_leaf_intersects_image(vaddr, level, r)) {
            uint64_t size = 1ULL << shift_at_level(level);
            uint64_t end = vaddr + size;
            if (end < vaddr)
                end = UINT64_MAX;
            PANIC("vmm: audit: level-%u large leaf 0x%lx-0x%lx intersects kernel %s",
                  level, (unsigned long)vaddr, (unsigned long)(end - 1),
                  intersecting_section(vaddr, level, r));
        }
        counts->other++;
        ASSERT_ALWAYS(!f.exec);
        return;
    }

    bool in_text = vaddr >= (uint64_t)r->text_start && vaddr < (uint64_t)r->text_end;
    bool in_rodata = vaddr >= (uint64_t)r->rodata_start && vaddr < (uint64_t)r->rodata_end;
    bool in_data = vaddr >= (uint64_t)r->data_start && vaddr < (uint64_t)r->data_end;

    if (in_text) {
        counts->text++;
        ASSERT_ALWAYS(f.exec);
        ASSERT_ALWAYS(!f.write);
        counts->exec++;
    } else if (in_rodata) {
        counts->rodata++;
        ASSERT_ALWAYS(!f.exec);
        ASSERT_ALWAYS(!f.write);
    } else if (in_data) {
        counts->data++;
        ASSERT_ALWAYS(!f.exec);
        ASSERT_ALWAYS(f.write);
    } else {
        counts->other++;
        ASSERT_ALWAYS(!f.exec);
    }

    ASSERT_ALWAYS(!(f.write && f.exec));
}

/*
 * Boot-time audit of the master kernel half (ADR-0007 tester list): every
 * present leaf must carry no U bit and must never be writable+executable.
 * Text must be executable and read-only, rodata NX and read-only, data/bss
 * writable and NX, and the executable-leaf count must equal the number of
 * text pages. The user half must be entirely zero. Panics on the first
 * defeated invariant.
 */
static void audit_kernel_half(void)
{
    struct arch_image_ranges r;
    arch_image_ranges(&r);

    struct audit_counts counts = {0, 0, 0, 0, 0};

    for (unsigned i = MM_PML4_KERNEL_FIRST; i < ARCH_MM4_TABLE_ENTRIES; i++) {
        uint64_t pml4e = arch_mm_table_read(s_master_pml4, i);
        struct arch_mm_entry_flags f4;
        arch_mm_entry_flags(pml4e, MM_LEVEL_PML4, &f4);
        if (!f4.present)
            continue;
        ASSERT_ALWAYS(arch_mm_entry_is_table(pml4e, MM_LEVEL_PML4));
        ASSERT_ALWAYS(!f4.user);

        uint64_t base = mm_canonical_base(i);

        for (unsigned j = 0; j < ARCH_MM4_TABLE_ENTRIES; j++) {
            uint64_t e3 = arch_mm_table_read(arch_mm_entry_phys(pml4e), j);
            struct arch_mm_entry_flags f3;
            arch_mm_entry_flags(e3, MM_LEVEL_PDPT, &f3);
            if (!f3.present)
                continue;
            ASSERT_ALWAYS(!f3.user);
            uint64_t v2 = base + ((uint64_t)j << ARCH_MM4_SHIFT_L3);

            if (f3.large) {
                audit_leaf(e3, MM_LEVEL_PDPT, v2, &r, &counts);
                continue;
            }

            for (unsigned k = 0; k < ARCH_MM4_TABLE_ENTRIES; k++) {
                uint64_t e2 = arch_mm_table_read(arch_mm_entry_phys(e3), k);
                struct arch_mm_entry_flags f2;
                arch_mm_entry_flags(e2, MM_LEVEL_PD, &f2);
                if (!f2.present)
                    continue;
                ASSERT_ALWAYS(!f2.user);
                uint64_t v3 = v2 + ((uint64_t)k << ARCH_MM4_SHIFT_L2);

                if (f2.large) {
                    audit_leaf(e2, MM_LEVEL_PD, v3, &r, &counts);
                    continue;
                }

                for (unsigned n = 0; n < ARCH_MM4_TABLE_ENTRIES; n++) {
                    uint64_t e1 = arch_mm_table_read(arch_mm_entry_phys(e2), n);
                    struct arch_mm_entry_flags f1;
                    arch_mm_entry_flags(e1, MM_LEVEL_PT, &f1);
                    if (!f1.present)
                        continue;
                    uint64_t v4 = v3 + ((uint64_t)n << ARCH_MM4_SHIFT_L1);
                    audit_leaf(e1, MM_LEVEL_PT, v4, &r, &counts);
                }
            }
        }
    }

    /* The user half of the master space must be empty (zeroed). */
    for (unsigned i = 0; i < MM_PML4_KERNEL_FIRST; i++)
        ASSERT_ALWAYS(arch_mm_table_read(s_master_pml4, i) == 0);

    size_t text_pages = (size_t)((align_up((uint64_t)r.text_end, PAGE_SIZE) -
                                  align_down((uint64_t)r.text_start, PAGE_SIZE)) /
                                 PAGE_SIZE);
    size_t rodata_pages = (size_t)((align_up((uint64_t)r.rodata_end, PAGE_SIZE) -
                                    align_down((uint64_t)r.rodata_start, PAGE_SIZE)) /
                                   PAGE_SIZE);
    size_t data_pages =
        (size_t)(((uint64_t)r.data_end - (uint64_t)r.data_start) / PAGE_SIZE);

    ASSERT_ALWAYS(counts.exec > 0);
    ASSERT_ALWAYS(counts.exec == counts.text);
    ASSERT_ALWAYS(counts.text == text_pages);
    ASSERT_ALWAYS(counts.rodata == rodata_pages);
    ASSERT_ALWAYS(counts.data == data_pages);

    log_info("vmm: audit clean (%zu leaves: %zu text RX, %zu rodata R, %zu data RW, "
             "%zu other NX; %zu executable, no U bit, no W|X)",
             counts.text + counts.rodata + counts.data + counts.other, counts.text,
             counts.rodata, counts.data, counts.other, counts.exec);
}

int vmm_map(uintptr_t vaddr, uint64_t paddr, size_t npages, unsigned flags)
{
    ASSERT_ALWAYS(s_vmm_initialised);
    if (npages == 0)
        return 0;

    int status = vmm_validate_range(vaddr, paddr, npages, flags);
    if (status != 0)
        return status;

    /*
     * Pre-validate the whole range before touching a table. This keeps
     * overlap (EEXIST) deterministic and means an allocation failure later
     * only needs to roll back translations, never a half-validated range.
     */
    for (size_t i = 0; i < npages; i++) {
        struct mm_query q;
        mm_query_mapping(s_master_pml4, vaddr + i * PAGE_SIZE, &q);
        if (q.present)
            return -EEXIST;
    }

    size_t mapped = 0;
    for (size_t i = 0; i < npages; i++) {
        uint64_t page_vaddr = vaddr + i * PAGE_SIZE;
        uint64_t leaf = arch_mm_entry_leaf(paddr + i * PAGE_SIZE,
                                           (flags & VMM_WRITE) != 0,
                                           (flags & VMM_EXEC) != 0, false, false);
        uint64_t pt = 0;
        enum mm_set_result result =
            mm_set_leaf(s_master_pml4, page_vaddr, leaf, false, &s_master_table_frames, &pt);
        if (result == MM_SET_EXISTS) {
            status = -EEXIST;
            break;
        }
        if (result == MM_SET_BLOCKED) {
            status = -EEXIST;
            break;
        }
        if (result == MM_SET_NOMEM) {
            status = -ENOMEM;
            break;
        }
        mapped++;
        arch_mm_invlpg(page_vaddr);
    }

    if (status != 0) {
        for (size_t i = 0; i < mapped; i++)
            (void)mm_clear_leaf(s_master_pml4, vaddr + i * PAGE_SIZE, false, NULL);
        return status;
    }
    return 0;
}

int vmm_unmap(uintptr_t vaddr, size_t npages)
{
    ASSERT_ALWAYS(s_vmm_initialised);
    if (npages == 0)
        return 0;

    if ((vaddr & PAGE_OFFSET_MASK) != 0 || vaddr < VMM_DYNMAP_BASE ||
        vaddr >= VMM_DYNMAP_END || npages > (SIZE_MAX / PAGE_SIZE))
        return -EINVAL;

    uint64_t bytes = (uint64_t)npages * PAGE_SIZE;
    uint64_t vaddr_end = (uint64_t)vaddr + bytes;
    if (vaddr_end < (uint64_t)vaddr || vaddr_end > VMM_DYNMAP_END)
        return -EINVAL;

    /*
     * Translations are removed but dynmap tables stay allocated: they were
     * preseeded before the first address space exists (ADR-0007), and
     * removing a top-level entry later would make future spaces miss it.
     * See vmm.h.
     */
    int status = 0;
    for (size_t i = 0; i < npages; i++) {
        uint64_t page_vaddr = vaddr + i * PAGE_SIZE;
        if (!mm_clear_leaf(s_master_pml4, page_vaddr, false, &s_master_table_frames))
            status = -ENOENT; /* range was not fully mapped */
    }
    return status;
}

bool vmm_resolve(uintptr_t vaddr, uint64_t *paddr_out)
{
    ASSERT_ALWAYS(s_vmm_initialised);
    return mm_translate(s_master_pml4, vaddr, paddr_out);
}

uint64_t vmm_master_pml4(void)
{
    return s_master_pml4;
}

static const char *expected_name(enum mm_expect expect)
{
    switch (expect) {
    case MM_EXPECT_RX:
        return "present RX (not writable)";
    case MM_EXPECT_RW_NX:
        return "present RW NX";
    case MM_EXPECT_R_NX:
        return "present NX";
    case MM_EXPECT_UNMAPPED:
        return "not present";
    default:
        return "unknown";
    }
}

static void add_spec(struct mm_preflight_spec *specs, size_t *count, size_t capacity,
                     uint64_t vaddr, enum mm_expect expect, const char *name)
{
    if (*count >= capacity)
        PANIC("vmm: pre-flight specification table overflow");
    specs[*count].vaddr = vaddr;
    specs[*count].expect = expect;
    specs[*count].name = name;
    (*count)++;
}

static void preflight_or_panic(const struct bootinfo *bi)
{
    struct arch_mm_kernel_tables tables;
    arch_mm_kernel_tables(&tables);

    uintptr_t stack_low = 0;
    uintptr_t stack_high = 0;
    arch_mm_boot_stack_bounds(&stack_low, &stack_high);
    uintptr_t rsp = arch_mm_read_rsp();

    struct mm_preflight_spec specs[32];
    size_t count = 0;

    /* Code that must still be executable after the CR3 switch. */
    add_spec(specs, &count, ARRAY_SIZE(specs), (uint64_t)(uintptr_t)&vmm_init, MM_EXPECT_RX,
             "vmm_init");
    add_spec(specs, &count, ARRAY_SIZE(specs), arch_mm_exception_dispatch_addr(), MM_EXPECT_RX,
             "exception dispatcher");
    add_spec(specs, &count, ARRAY_SIZE(specs), arch_mm_exception_stub_addr(8), MM_EXPECT_RX,
             "#DF entry stub");
    add_spec(specs, &count, ARRAY_SIZE(specs), arch_mm_exception_stub_addr(14), MM_EXPECT_RX,
             "#PF entry stub");
    add_spec(specs, &count, ARRAY_SIZE(specs), (uint64_t)(uintptr_t)&panic_at, MM_EXPECT_RX,
             "panic");
    add_spec(specs, &count, ARRAY_SIZE(specs), (uint64_t)(uintptr_t)&arch_early_console_putc,
             MM_EXPECT_RX, "serial output");
    add_spec(specs, &count, ARRAY_SIZE(specs), (uint64_t)(uintptr_t)&arch_halt_forever,
             MM_EXPECT_RX, "halt loop");

    /* Writable stacks and descriptor tables. */
    add_spec(specs, &count, ARRAY_SIZE(specs), (uint64_t)rsp, MM_EXPECT_RW_NX,
             "current RSP");
    add_spec(specs, &count, ARRAY_SIZE(specs), (uint64_t)stack_low, MM_EXPECT_RW_NX,
             "boot stack bottom");
    if (stack_high >= 16)
        add_spec(specs, &count, ARRAY_SIZE(specs), (uint64_t)(stack_high - 16),
                 MM_EXPECT_RW_NX, "boot stack top");

    add_spec(specs, &count, ARRAY_SIZE(specs), (uint64_t)tables.gdt_base, MM_EXPECT_RW_NX,
             "GDT");
    if (tables.gdt_size > 1)
        add_spec(specs, &count, ARRAY_SIZE(specs),
                 (uint64_t)tables.gdt_base + tables.gdt_size - 1, MM_EXPECT_RW_NX,
                 "GDT last byte");
    add_spec(specs, &count, ARRAY_SIZE(specs), (uint64_t)tables.idt_base, MM_EXPECT_RW_NX,
             "IDT");
    if (tables.idt_size > 1)
        add_spec(specs, &count, ARRAY_SIZE(specs),
                 (uint64_t)tables.idt_base + tables.idt_size - 1, MM_EXPECT_RW_NX,
                 "IDT last byte");
    add_spec(specs, &count, ARRAY_SIZE(specs), (uint64_t)tables.tss_base, MM_EXPECT_RW_NX,
             "TSS");
    if (tables.tss_size > 1)
        add_spec(specs, &count, ARRAY_SIZE(specs),
                 (uint64_t)tables.tss_base + tables.tss_size - 1, MM_EXPECT_RW_NX,
                 "TSS last byte");

    for (size_t i = 0; i < ARRAY_SIZE(tables.ist_top); i++) {
        if (tables.ist_top[i] >= 16)
            add_spec(specs, &count, ARRAY_SIZE(specs), (uint64_t)(tables.ist_top[i] - 16),
                     MM_EXPECT_RW_NX, "IST stack top");
    }

    /* The HHDM aliases used by page-table and PMM code must be NX. */
    uint64_t bitmap_alias = 0;
    uint64_t pml4_alias = 0;
    if (add_overflows_u64(bi->hhdm_offset, pmm_bitmap_phys(), &bitmap_alias) ||
        add_overflows_u64(bi->hhdm_offset, s_master_pml4, &pml4_alias))
        PANIC("vmm: HHDM alias overflows during pre-flight");
    add_spec(specs, &count, ARRAY_SIZE(specs), bitmap_alias, MM_EXPECT_RW_NX,
             "HHDM PMM bitmap");
    add_spec(specs, &count, ARRAY_SIZE(specs), pml4_alias, MM_EXPECT_RW_NX,
             "HHDM master PML4");

    /* The dynmap must start with no translation; the self-test adds one. */
    add_spec(specs, &count, ARRAY_SIZE(specs), VMM_DYNMAP_BASE, MM_EXPECT_UNMAPPED,
             "unmapped dynmap");

    struct mm_preflight_failure failure;
    if (!mm_check_mappings(s_master_pml4, specs, count, &failure)) {
        const char *actual = failure.actual.present
                                 ? (failure.actual.leaf ? "present leaf" : "present non-leaf")
                                 : "not present";
        PANIC("vmm: pre-flight failed for %s at 0x%016lx: expected %s, got %s "
              "(entry 0x%016lx, level %u)",
              failure.name, (unsigned long)failure.vaddr, expected_name(failure.expect),
              actual, (unsigned long)failure.actual.entry, failure.actual.level);
    }

    log_info("vmm: pre-flight check passed (%zu critical mappings)", count);
}

/*
 * In-kernel mapping self-test (runs after the CR3 switch): map a fresh PMM
 * frame into the dynmap window, resolve it, write through it (would fault
 * if the mapping were wrong), assert W^X rejection, then unmap and return
 * the frame to the PMM.
 */
static void vmm_mapping_self_test(void)
{
    const uintptr_t scratch = VMM_DYNMAP_BASE;
    uintptr_t frame = pmm_alloc_frame();
    ASSERT_ALWAYS(frame != PADDR_INVALID && frame != 0);

    ASSERT_ALWAYS(vmm_map(scratch, frame, 1, VMM_WRITE) == 0);

    uint64_t resolved = 0;
    ASSERT_ALWAYS(vmm_resolve(scratch, &resolved));
    ASSERT_ALWAYS(resolved == frame);
    ASSERT_ALWAYS(vmm_resolve(scratch + 0x123, &resolved));
    ASSERT_ALWAYS(resolved == frame + 0x123);

    volatile uint64_t *va = (volatile uint64_t *)(scratch + 0x8);
    *va = 0x5AA55AA5CAFEBABEULL;
    ASSERT_ALWAYS(*va == 0x5AA55AA5CAFEBABEULL);

    ASSERT_ALWAYS(vmm_check_flags(VMM_WRITE | VMM_EXEC) != 0);
    ASSERT_ALWAYS(vmm_map(scratch, frame, 1, VMM_WRITE | VMM_EXEC) == -EINVAL);
    ASSERT_ALWAYS(vmm_map(scratch, frame, 1, VMM_WRITE) == -EEXIST);

    ASSERT_ALWAYS(vmm_unmap(scratch, 1) == 0);
    ASSERT_ALWAYS(!vmm_resolve(scratch, &resolved));
    ASSERT_ALWAYS(vmm_unmap(scratch, 1) == -ENOENT);
    pmm_free_frame(frame);

    log_info("vmm: mapping self-test passed (map/resolve/write/unmap round-trip)");
}

void vmm_init(const struct bootinfo *bi)
{
    ASSERT_ALWAYS(bi != NULL);
    ASSERT_ALWAYS(!s_vmm_initialised);

    check_hhdm_bounds(bi);
    arch_mm_set_hhdm_base(bi->hhdm_offset);
    arch_mm_cpu_memory_init();

    /* Seed the master PML4 from the running bootloader tables (CR3). */
    uint64_t boot_pml4 = arch_mm_read_cr3();
    s_master_pml4 = mm_alloc_table(&s_master_table_frames);

    for (unsigned i = MM_PML4_KERNEL_FIRST; i < ARCH_MM4_TABLE_ENTRIES; i++) {
        uint64_t entry = arch_mm_table_read(boot_pml4, i);
        struct arch_mm_entry_flags f;
        arch_mm_entry_flags(entry, MM_LEVEL_PML4, &f);
        if (!f.present)
            continue;

        uint64_t vaddr_base = mm_canonical_base(i);
        uint64_t new_entry;
        if (arch_mm_entry_is_table(entry, MM_LEVEL_PML4)) {
            uint64_t new_pdpt = copy_table(arch_mm_entry_phys(entry), MM_LEVEL_PDPT, vaddr_base);
            new_entry = arch_mm_entry_table(new_pdpt, false);
        } else {
            /*
             * PS is reserved in a PML4 entry; 1 GiB leaves belong at the
             * PDPT level. Treating this as a 512 GiB leaf would be wrong.
             */
            PANIC("vmm: PML4 entry %u is present but not a table (PS reserved)", i);
        }
        arch_mm_table_write(s_master_pml4, i, new_entry);
    }

    preseed_region_slot(VMM_KERN_HEAP_BASE, "kernel heap");
    preseed_region_slot(VMM_KERN_STACKS_BASE, "kernel stacks");
    preseed_region_slot(VMM_DYNMAP_BASE, "dynmap");

    audit_kernel_half();
    preflight_or_panic(bi);

    arch_mm_set_cr3(s_master_pml4);
    s_vmm_initialised = true;

    log_info("vmm: master kernel page tables active (CR3 0x%016lx, %zu table frames)",
             (unsigned long)s_master_pml4, s_master_table_frames);
    log_info("vmm: kernel half re-protected (text RX, HHDM/data NX, W^X enforced)");

    vmm_mapping_self_test();
}
