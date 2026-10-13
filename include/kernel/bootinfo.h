/*
 * bootinfo.h - kernel-owned copy of the information the bootloader provides.
 *
 * The bootloader's response structures live in bootloader-reclaimable memory
 * whose lifetime is limited. bootinfo_capture() copies everything the kernel
 * needs, by value, into a kernel-owned struct; the rest of the kernel only uses
 * this header and never holds bootloader pointers (AR-007, docs/BOOT.md).
 */
#ifndef ATAOS_KERNEL_BOOTINFO_H
#define ATAOS_KERNEL_BOOTINFO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BOOTINFO_MAX_MEMMAP 128
#define BOOTINFO_STR_MAX 64

enum bootinfo_mem_type {
    BOOT_MEM_USABLE = 0,
    BOOT_MEM_RESERVED,
    BOOT_MEM_ACPI_RECLAIMABLE,
    BOOT_MEM_ACPI_NVS,
    BOOT_MEM_BAD,
    BOOT_MEM_BOOTLOADER_RECLAIMABLE,
    BOOT_MEM_KERNEL_AND_MODULES,
    BOOT_MEM_FRAMEBUFFER,
    BOOT_MEM_RESERVED_MAPPED,
    BOOT_MEM_UNKNOWN,
};

enum bootinfo_firmware {
    BOOT_FW_UNKNOWN = 0,
    BOOT_FW_BIOS,
    BOOT_FW_UEFI32,
    BOOT_FW_UEFI64,
};

struct bootinfo_mem_region {
    uint64_t base;
    uint64_t length;
    enum bootinfo_mem_type type;
};

struct bootinfo_framebuffer {
    uint64_t address; /* virtual address (inside the HHDM) as given by the bootloader */
    uint64_t width;
    uint64_t height;
    uint64_t pitch;
    uint16_t bpp;
};

struct bootinfo {
    uint64_t hhdm_offset;
    enum bootinfo_firmware firmware;
    char bootloader_name[BOOTINFO_STR_MAX];
    char bootloader_version[BOOTINFO_STR_MAX];

    bool kernel_addr_valid;
    uint64_t kernel_phys_base;
    uint64_t kernel_virt_base;

    size_t memmap_count;
    struct bootinfo_mem_region memmap[BOOTINFO_MAX_MEMMAP];

    size_t framebuffer_count;
    struct bootinfo_framebuffer framebuffer; /* first framebuffer only */

    bool rsdp_present;
    uint64_t rsdp_address; /* virtual address (inside the HHDM) as given by the bootloader */
};

/*
 * Capture boot information. Panics with a message naming the missing item if a
 * required response (base revision, HHDM, memory map) is absent or the memory
 * map does not fit in BOOTINFO_MAX_MEMMAP. Must be called once, early.
 */
void bootinfo_capture(void);

/* Pointer to the captured information; valid after bootinfo_capture(). */
const struct bootinfo *bootinfo_get(void);

/* Human-readable names for logging. Never return NULL. */
const char *bootinfo_mem_type_name(enum bootinfo_mem_type type);
const char *bootinfo_firmware_name(enum bootinfo_firmware fw);

#endif /* ATAOS_KERNEL_BOOTINFO_H */
