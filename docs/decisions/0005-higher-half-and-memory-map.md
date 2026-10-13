# ADR-0005: Kernel higher-half address and memory-map policy

- **Status:** accepted (link address); policy items accepted as *design* until M3/M4 verify them
- **Date:** 2026-10-13

## Context and problem

Choose the kernel link address, code model and the way the kernel uses the
bootloader's memory map and direct map.

## Considered alternatives

1. **Link at `0xffffffff80000000`, `-mcmodel=kernel`, no KASLR** — the standard
   top-2-GiB layout; Limine natively supports it; simple relocations.
2. **Position-independent kernel with KASLR** — better hardening; needs
   relocation handling, complicates early debugging.
3. **Lower-half / identity-mapped kernel** — conflicts with user half and
   Limine base revision ≥ 1 (no identity map).

## Decision

* Link address `0xffffffff80000000` (`KERNEL_VMA`), `-mcmodel=kernel`, `-fno-pic`;
  `PT_LOAD` segments aligned to 4 KiB with distinct R / RX / RW permissions
  (`-z separate-code` equivalent in the linker script: `.text`, `.rodata`,
  `.data/.bss` on separate pages).
* Physical load location is chosen by Limine (physically contiguous); the kernel
  learns it from the executable-address request and never assumes it.
* KASLR off (`limine.conf: kaslr: no`, `randomise_hhdm_base: no`), paging pinned
  to 4-level. HHDM base is read from the bootloader.
* Memory map: copied into kernel-owned storage; frame 0 never allocated;
  `BOOTLOADER_RECLAIMABLE` reclaimed only after boot-info retirement and after
  the kernel has its own page tables (MEMORY_LAYOUT §3).
* Expected sections: `.text`, `.rodata`, `.data`, `.bss`, `.limine_requests*`
  (kept), `.note`/`.eh_frame` discarded except debug info.

## Rationale

It is the best-supported combination across compiler, linker, and bootloader and
the least surprising when debugging.

## Trade-offs and consequences

No KASLR hardening. Kernel image limited to < 1 GiB planned, 2 GiB hard limit by
code model. Fixed address makes symbol addresses stable in GDB.

## Testing and verification implications

`readelf -lS build/*/ataos.elf` shows `PT_LOAD` vaddrs ≥ `0xffffffff80000000`;
boot log prints the Limine-reported virtual base which must equal the link
address; the memory-map invariants are tested in M3.

## Conditions that could justify revisiting

Enabling KASLR for hardening; kernel growing beyond the reserved region; a
bootloader change.
