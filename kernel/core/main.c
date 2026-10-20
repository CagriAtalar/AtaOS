/*
 * main.c - kernel entry (called from arch/x86_64/boot/entry.S).
 *
 * M4 init order: early console, GDT/TSS, IDT, boot information, PMM, master
 * kernel page tables (CR3 switch and self-test), then the kernel heap. The
 * final boot marker is printed before halting safely; see
 * docs/ARCHITECTURE.md for the planned sequence beyond M4.
 */
#include "buildinfo.h"
#include "kernel/arch.h"
#include "kernel/bootinfo.h"
#include "kernel/kernel.h"
#include "kernel/log.h"
#include "kernel/panic.h"
#include "kernel/pmm.h"
#include "kernel/vmm.h"
#include "kernel/heap.h"

static void report_memory_map(const struct bootinfo *bi)
{
    uint64_t usable = 0;
    uint64_t reclaimable = 0;
    uint64_t other = 0;

    log_info("memory map: %zu entries", bi->memmap_count);
    for (size_t i = 0; i < bi->memmap_count; i++) {
        const struct bootinfo_mem_region *r = &bi->memmap[i];

        log_info("  0x%016lx - 0x%016lx %s", r->base, r->base + r->length,
                 bootinfo_mem_type_name(r->type));

        if (r->type == BOOT_MEM_USABLE)
            usable += r->length;
        else if (r->type == BOOT_MEM_BOOTLOADER_RECLAIMABLE)
            reclaimable += r->length;
        else
            other += r->length;
    }
    log_info("memory: usable %lu KiB, bootloader-reclaimable %lu KiB, other %lu KiB",
             usable / 1024, reclaimable / 1024, other / 1024);
}

static void report_boot_info(const struct bootinfo *bi)
{
    log_info("bootloader: %s %s", bi->bootloader_name, bi->bootloader_version);
    log_info("firmware: %s", bootinfo_firmware_name(bi->firmware));
    log_info("hhdm base: 0x%016lx", bi->hhdm_offset);

    if (bi->kernel_addr_valid)
        log_info("kernel: phys 0x%lx virt 0x%lx", bi->kernel_phys_base, bi->kernel_virt_base);
    else
        log_warn("kernel address not reported by bootloader");

    report_memory_map(bi);

    if (bi->framebuffer_count > 0)
        log_info("framebuffer: %lux%lux%u pitch %lu (count %zu)", bi->framebuffer.width,
                 bi->framebuffer.height, (unsigned)bi->framebuffer.bpp, bi->framebuffer.pitch,
                 bi->framebuffer_count);
    else
        log_info("framebuffer: none");

    if (bi->rsdp_present)
        log_info("rsdp: 0x%lx", bi->rsdp_address);
    else
        log_info("rsdp: none");
}

void kmain(void)
{
    arch_early_console_init();

    log_info("AtaOS %s (%s, git %s) x86_64", ATAOS_VERSION, ATAOS_BUILD_FLAVOR, ATAOS_GIT_REV);

    /* Initialize custom GDT and TSS (M2 Task 1) */
    arch_gdt_init();

    /* Initialize minimal safe IDT (M2 Task 2) */
    arch_idt_init();

    bootinfo_capture();
    report_boot_info(bootinfo_get());

    /* Initialize physical memory manager (M3) */
    pmm_init(bootinfo_get());

    /* Build master kernel page tables and switch CR3 (M4) */
    vmm_init(bootinfo_get());

    /* Initialize kernel heap allocator (M3; uses the HHDM kept by vmm_init) */
    heap_init();

#ifdef ATAOS_TEST_PANIC
    /* Deliberate failure used only by the panic-detection test (TEST_PANIC=1). */
    PANIC("deliberate test panic (ATAOS_TEST_PANIC)");
#endif

#if defined(ATAOS_TEST_FAULT_DE)
    arch_test_fault_de();
#elif defined(ATAOS_TEST_FAULT_UD)
    arch_test_fault_ud();
#elif defined(ATAOS_TEST_FAULT_GP)
    arch_test_fault_gp();
#elif defined(ATAOS_TEST_FAULT_PF)
    arch_test_fault_pf();
#elif defined(ATAOS_TEST_FAULT_DF)
    arch_test_fault_df();
#elif defined(ATAOS_TEST_FAULT)
    arch_test_fault_ud();
#endif

    log_info("boot complete");

#ifdef ATAOS_TEST_BUILD
    arch_test_exit(ARCH_TEST_EXIT_PASS);
#endif
    arch_halt_forever();
}
