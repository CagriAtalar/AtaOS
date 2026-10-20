/*
 * image_ranges.c - expose the kernel image section ranges to kernel/mm (M4).
 *
 * The symbols are defined by arch/x86_64/linker.ld, which page-aligns the
 * text, rodata and data/bss boundaries, so each range can be re-protected
 * page by page without splitting a section across a page edge.
 */
#include "kernel/arch.h"

/* Defined by arch/x86_64/linker.ld */
extern char __text_start[];
extern char __text_end[];
extern char __rodata_start[];
extern char __rodata_end[];
extern char __data_start[];
extern char __data_end[];

void arch_image_ranges(struct arch_image_ranges *out)
{
    if (!out)
        return;

    out->text_start = (uintptr_t)__text_start;
    out->text_end = (uintptr_t)__text_end;
    out->rodata_start = (uintptr_t)__rodata_start;
    out->rodata_end = (uintptr_t)__rodata_end;
    out->data_start = (uintptr_t)__data_start;
    out->data_end = (uintptr_t)__data_end;
}
