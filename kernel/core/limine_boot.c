/*
 * limine_boot.c - Limine boot protocol requests and boot-info capture.
 *
 * Protocol: Limine base revision 6 (see docs/BOOT.md, ADR-0001).
 * All bootloader-provided pointers are consumed here and copied into the
 * kernel-owned struct bootinfo; nothing outside this file sees them.
 */
#include <limine.h>

#include "kernel/bootinfo.h"
#include "kernel/compiler.h"
#include "kernel/panic.h"
#include "kernel/string.h"

#define LIMINE_BASE_REVISION_WANTED 6

/*
 * Requests live between the start and end markers so the bootloader only scans
 * this region. They must be volatile: the bootloader fills in the responses
 * before the kernel runs and the compiler must not assume they are NULL.
 */
ATAOS_USED ATAOS_SECTION(".limine_requests_start") static volatile uint64_t requests_start[4] =
    LIMINE_REQUESTS_START_MARKER;

ATAOS_USED ATAOS_SECTION(".limine_requests") static volatile uint64_t base_revision[3] =
    LIMINE_BASE_REVISION(LIMINE_BASE_REVISION_WANTED);

ATAOS_USED ATAOS_SECTION(".limine_requests") static volatile struct limine_bootloader_info_request
    bootloader_info_req = {.id = LIMINE_BOOTLOADER_INFO_REQUEST_ID, .revision = 0};

ATAOS_USED ATAOS_SECTION(".limine_requests") static volatile struct limine_firmware_type_request
    firmware_type_req = {.id = LIMINE_FIRMWARE_TYPE_REQUEST_ID, .revision = 0};

ATAOS_USED ATAOS_SECTION(".limine_requests") static volatile struct limine_hhdm_request hhdm_req = {
    .id = LIMINE_HHDM_REQUEST_ID, .revision = 0};

ATAOS_USED ATAOS_SECTION(".limine_requests") static volatile struct limine_memmap_request
    memmap_req = {.id = LIMINE_MEMMAP_REQUEST_ID, .revision = 0};

ATAOS_USED ATAOS_SECTION(".limine_requests") static volatile struct limine_framebuffer_request
    framebuffer_req = {.id = LIMINE_FRAMEBUFFER_REQUEST_ID, .revision = 0};

ATAOS_USED ATAOS_SECTION(".limine_requests") static volatile struct limine_rsdp_request rsdp_req = {
    .id = LIMINE_RSDP_REQUEST_ID, .revision = 0};

ATAOS_USED ATAOS_SECTION(".limine_requests") static volatile struct limine_executable_address_request
    exec_addr_req = {.id = LIMINE_EXECUTABLE_ADDRESS_REQUEST_ID, .revision = 0};

ATAOS_USED ATAOS_SECTION(".limine_requests_end") static volatile uint64_t requests_end[2] =
    LIMINE_REQUESTS_END_MARKER;

static struct bootinfo g_bootinfo;

static enum bootinfo_mem_type convert_mem_type(uint64_t t)
{
    switch (t) {
    case LIMINE_MEMMAP_USABLE:
        return BOOT_MEM_USABLE;
    case LIMINE_MEMMAP_RESERVED:
        return BOOT_MEM_RESERVED;
    case LIMINE_MEMMAP_ACPI_RECLAIMABLE:
        return BOOT_MEM_ACPI_RECLAIMABLE;
    case LIMINE_MEMMAP_ACPI_NVS:
        return BOOT_MEM_ACPI_NVS;
    case LIMINE_MEMMAP_BAD_MEMORY:
        return BOOT_MEM_BAD;
    case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE:
        return BOOT_MEM_BOOTLOADER_RECLAIMABLE;
    case LIMINE_MEMMAP_EXECUTABLE_AND_MODULES:
        return BOOT_MEM_KERNEL_AND_MODULES;
    case LIMINE_MEMMAP_FRAMEBUFFER:
        return BOOT_MEM_FRAMEBUFFER;
    case LIMINE_MEMMAP_RESERVED_MAPPED:
        return BOOT_MEM_RESERVED_MAPPED;
    default:
        return BOOT_MEM_UNKNOWN;
    }
}

static enum bootinfo_firmware convert_firmware(uint64_t t)
{
    switch (t) {
    case LIMINE_FIRMWARE_TYPE_X86BIOS:
        return BOOT_FW_BIOS;
    case LIMINE_FIRMWARE_TYPE_EFI32:
        return BOOT_FW_UEFI32;
    case LIMINE_FIRMWARE_TYPE_EFI64:
        return BOOT_FW_UEFI64;
    default:
        return BOOT_FW_UNKNOWN;
    }
}

void bootinfo_capture(void)
{
    struct bootinfo *bi = &g_bootinfo;

    memset(bi, 0, sizeof(*bi));

    if (!LIMINE_BASE_REVISION_SUPPORTED(base_revision))
        PANIC("bootloader does not support Limine base revision %d",
              LIMINE_BASE_REVISION_WANTED);

    if (hhdm_req.response == NULL)
        PANIC("required Limine response missing: HHDM");
    bi->hhdm_offset = hhdm_req.response->offset;

    if (memmap_req.response == NULL)
        PANIC("required Limine response missing: memory map");
    const struct limine_memmap_response *mm = memmap_req.response;
    if (mm->entry_count > BOOTINFO_MAX_MEMMAP)
        PANIC("memory map has %lu entries, capacity is %d", (unsigned long)mm->entry_count,
              BOOTINFO_MAX_MEMMAP);
    for (uint64_t i = 0; i < mm->entry_count; i++) {
        const struct limine_memmap_entry *e = mm->entries[i];
        bi->memmap[i].base = e->base;
        bi->memmap[i].length = e->length;
        bi->memmap[i].type = convert_mem_type(e->type);
    }
    bi->memmap_count = (size_t)mm->entry_count;

    if (bootloader_info_req.response != NULL) {
        strlcpy(bi->bootloader_name, bootloader_info_req.response->name,
                sizeof(bi->bootloader_name));
        strlcpy(bi->bootloader_version, bootloader_info_req.response->version,
                sizeof(bi->bootloader_version));
    } else {
        strlcpy(bi->bootloader_name, "unknown", sizeof(bi->bootloader_name));
        strlcpy(bi->bootloader_version, "unknown", sizeof(bi->bootloader_version));
    }

    if (firmware_type_req.response != NULL)
        bi->firmware = convert_firmware(firmware_type_req.response->firmware_type);

    if (exec_addr_req.response != NULL) {
        bi->kernel_addr_valid = true;
        bi->kernel_phys_base = exec_addr_req.response->physical_base;
        bi->kernel_virt_base = exec_addr_req.response->virtual_base;
    }

    if (framebuffer_req.response != NULL && framebuffer_req.response->framebuffer_count > 0) {
        const struct limine_framebuffer *fb = framebuffer_req.response->framebuffers[0];
        bi->framebuffer_count = (size_t)framebuffer_req.response->framebuffer_count;
        bi->framebuffer.address = (uint64_t)(uintptr_t)fb->address;
        bi->framebuffer.width = fb->width;
        bi->framebuffer.height = fb->height;
        bi->framebuffer.pitch = fb->pitch;
        bi->framebuffer.bpp = fb->bpp;
    }

    if (rsdp_req.response != NULL && rsdp_req.response->address != NULL) {
        bi->rsdp_present = true;
        bi->rsdp_address = (uint64_t)(uintptr_t)rsdp_req.response->address;
    }
}

const struct bootinfo *bootinfo_get(void)
{
    return &g_bootinfo;
}

const char *bootinfo_mem_type_name(enum bootinfo_mem_type type)
{
    switch (type) {
    case BOOT_MEM_USABLE:
        return "usable";
    case BOOT_MEM_RESERVED:
        return "reserved";
    case BOOT_MEM_ACPI_RECLAIMABLE:
        return "acpi-reclaimable";
    case BOOT_MEM_ACPI_NVS:
        return "acpi-nvs";
    case BOOT_MEM_BAD:
        return "bad";
    case BOOT_MEM_BOOTLOADER_RECLAIMABLE:
        return "bootloader-reclaimable";
    case BOOT_MEM_KERNEL_AND_MODULES:
        return "kernel/modules";
    case BOOT_MEM_FRAMEBUFFER:
        return "framebuffer";
    case BOOT_MEM_RESERVED_MAPPED:
        return "reserved-mapped";
    default:
        return "unknown";
    }
}

const char *bootinfo_firmware_name(enum bootinfo_firmware fw)
{
    switch (fw) {
    case BOOT_FW_BIOS:
        return "BIOS";
    case BOOT_FW_UEFI32:
        return "UEFI32";
    case BOOT_FW_UEFI64:
        return "UEFI64";
    default:
        return "unknown";
    }
}
