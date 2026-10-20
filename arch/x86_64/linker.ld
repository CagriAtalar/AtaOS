/*
 * linker.ld - AtaOS x86_64 kernel linker script.
 *
 * Link address 0xffffffff80000000 (top 2 GiB, required by -mcmodel=kernel and
 * matching Limine's higher-half kernel mapping). Three page-aligned PT_LOAD
 * segments give text R+X, rodata R, data/bss R+W (no W+X anywhere). KEEPed
 * .limine_requests* sections hold the bootloader request area.
 * See docs/MEMORY_LAYOUT.md and ADR-0005.
 */
OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)

PHDRS
{
    text   PT_LOAD FLAGS(5); /* R + X */
    rodata PT_LOAD FLAGS(4); /* R     */
    data   PT_LOAD FLAGS(6); /* R + W */
}

SECTIONS
{
    . = 0xffffffff80000000;
    __kernel_start = .;

    .text : ALIGN(4096) {
        __text_start = .;
        *(.text.boot)
        *(.text .text.*)
        __text_end = .;
    } :text

    . = ALIGN(4096);
    .rodata : {
        __rodata_start = .;
        *(.rodata .rodata.*)
        __rodata_end = .;
    } :rodata

    . = ALIGN(4096);
    .data : {
        __data_start = .;
        *(.data .data.*)
    } :data

    .limine_requests : {
        KEEP(*(.limine_requests_start))
        KEEP(*(.limine_requests))
        KEEP(*(.limine_requests_end))
    } :data

    .bss : {
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        __bss_end = .;
    } :data

    . = ALIGN(4096);
    /*
     * __data_end covers the whole writable image (.data + .limine_requests
     * + .bss): kernel/mm re-protects [__data_start, __data_end) as RW+NX in
     * its own page tables (M4). __kernel_end is the page-aligned image end.
     */
    __data_end = .;
    __kernel_end = .;

    /DISCARD/ : {
        *(.eh_frame*)
        *(.note .note.*)
        *(.comment)
    }
}
