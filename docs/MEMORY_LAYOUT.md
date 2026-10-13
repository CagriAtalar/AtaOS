# Memory Layout

> **Everything below except the kernel image link address and the Limine
> mapping facts is `PLANNED`.** Nothing in this document is implemented yet
> beyond M1 (kernel linked at `0xffffffff80000000`).

## 1. Assumptions

* x86_64, 4-level paging (48-bit virtual addresses; Limine request pinned to
  `4level`, 5-level is a non-goal). Canonical addresses: bits 63..47 are equal.
* Kernel built with `-mcmodel=kernel`: all kernel symbols must live in the top
  2 GiB (`0xffffffff80000000..0xffffffffffffffff`) so 32-bit sign-extended
  relocations work. Source of the constraint: SysV AMD64 ABI code models; Limine
  independently maps executables at or above `0xffffffff80000000`
  (PROTOCOL.md, "Memory Layout at Entry").
* KASLR and HHDM randomisation are **off** (`kaslr`/`randomise_hhdm_base: no` in
  `limine.conf`), for debuggability. The kernel still reads the HHDM base from
  the bootloader and never hard-codes it.
* Base revision **6** requested: HHDM maps only usable, bootloader-reclaimable,
  executable/modules, framebuffer, reserved-mapped, ACPI reclaimable and NVS
  regions. Reserved/bad memory is **not** in the HHDM and must not be touched
  through it.

## 2. Virtual address map (planned)

```
0xffffffffffffffff  +---------------------------+
                    | reserved (kernel image    |
0xffffffffc0000000  |  growth, fixmap, modules) |
                    +---------------------------+
                    | KERNEL IMAGE (.text .rodata .data .bss)
0xffffffff80000000  +---------------------------+  <- link address (IMPLEMENTED)
                    | unmapped / reserved       |
0xffffc20000000000  +---------------------------+
                    | kernel stacks region      |  guard page below each stack
0xffffc10000000000  +---------------------------+  (planned, 1 TiB)
                    | kernel heap region        |
0xffffc00000000000  +---------------------------+  (planned, 1 TiB)
                    | HHDM (bootloader-chosen   |
                    |  base; default            |
0xffff800000000000  +---------------------------+  0xffff800000000000; up to 64 TiB)
                    | NON-CANONICAL HOLE        |
0x0000800000000000  +---------------------------+
                    | user stack top (planned)  |
0x00007ffffffff000  +---------------------------+  top page reserved/unmapped
                    | user mmap region (later)  |
                    | user heap (grows up)      |
                    | user image (ELF segments) |
0x0000000000400000  +---------------------------+  default ELF base
                    | null guard (unmapped)     |
0x0000000000000000  +---------------------------+
```

| Region | Range (planned) | Size | Ownership | Perms |
|---|---|---|---|---|
| Null guard | `0 .. 0x3fffff` | 4 MiB | none; never mapped | — |
| User image | from `0x400000` | per ELF | process | per segment (R, RX, RW; never RWX) |
| User heap | above image, grows up | bounded by rlimit-like cap | process | RW, NX |
| User stack | ends `0x7fffffffe000` | 1 MiB default + 1 guard page below | process, owned by thread | RW, NX |
| HHDM | bootloader-chosen | up to 64 TiB | kernel, shared by all spaces | RW, NX (`RX` never) |
| Kernel heap | `0xffffc00000000000` + | 1 TiB reserve | kernel | RW, NX |
| Kernel stacks | `0xffffc10000000000` + | 16 KiB each + guard | thread | RW, NX |
| Kernel image | `0xffffffff80000000` + | ≈ small; 1 GiB reserved | kernel | text RX, rodata R, data/bss RW+NX |

Overlap check: HHDM start `0xffff800000000000` + 64 TiB (`0x400000000000`) =
`0xffffc00000000000` = kernel heap start; the regions are adjacent, not
overlapping. If the bootloader selects a different HHDM base the kernel must
verify at M3/M4 that `hhdm_base + max_phys` does not exceed
`0xffffc00000000000`, and `PANIC` otherwise.

Until M4 the kernel uses Limine's page tables unchanged.

## 3. Physical memory ownership (planned, M3)

Limine memory-map entry types and the kernel policy:

| Limine type | Policy |
|---|---|
| `USABLE` | Free frames for the PMM (page-aligned inward: base rounded up, end rounded down) |
| `BOOTLOADER_RECLAIMABLE` | Reserved until **boot-info retirement**; after the kernel has copied everything it needs and switched to its own page tables, the PMM may add these frames |
| `EXECUTABLE_AND_MODULES` | Reserved permanently (kernel image) / until module consumed |
| `FRAMEBUFFER` | Reserved; mapped write-combining later |
| `ACPI_RECLAIMABLE` | Reserved until ACPI tables are parsed and copied; then may be reclaimed |
| `ACPI_NVS`, `RESERVED`, `RESERVED_MAPPED`, `BAD_MEMORY` | Never allocated |

Invariants (PMM):

1. A frame is handed out only if it lies fully inside a `USABLE` (or retired
   reclaimable) region and its bitmap bit is clear.
2. Frame 0 (`0x0..0xfff`) is never allocated (null-pointer hardening), even if
   reported usable.
3. All addresses are 4 KiB aligned; sizes and counts are checked for overflow
   (`base + length` must not wrap).
4. Free of a frame not currently allocated is a bug: assert in debug builds.
5. Allocation failure returns `PADDR_INVALID`; the PMM never panics for
   exhaustion.
6. Accounting: `total`, `free`, `reserved`, `used_kernel`; leak test = counters
   return to starting values after an alloc/free sweep.

Bootloader response pointers are **not** retained: `bootinfo.h` accessors copy
values out into kernel-owned structures (`struct bootinfo`) during early init.
The memory map is copied into a fixed-size kernel array
(`BOOTINFO_MAX_MEMMAP`, entries are 24 bytes; excess entries cause a panic in
M1 rather than silent truncation). Response memory is bootloader-reclaimable
and remains valid only until retirement.

## 4. Direct map (HHDM)

* Purpose: kernel access to any mapped physical frame as
  `hhdm_base + paddr` without per-use mappings (page tables, frame zeroing, DMA
  buffers, initrd).
* Limitation: only regions listed in §1 are mapped (base revision ≥ 3). The
  kernel must never convert an unmapped physical address into an HHDM pointer
  (`phys_to_hhdm` asserts the address is in a mapped region in debug builds).
* Mapped RW + executable by Limine; the kernel's own tables (M4) remap it NX.

## 5. Page-size and permission policy (planned, M4)

* 4 KiB pages everywhere initially. 2 MiB pages may be used for the HHDM after
  measurement. 1 GiB pages are not used.
* Permissions: least privilege. `vmm_map(..., flags)` rejects `WRITE|EXEC`.
  Kernel text `RX`, rodata `R`, data/bss/stacks/heap `RW+NX`. User pages always
  carry the U bit; kernel pages never do. NXE enabled in `EFER`; `CR0.WP` set so
  the kernel respects read-only pages; SMEP/SMAP enabled when CPUID allows and
  SMAP's `stac/clac` bracketed only inside `copy_*_user`.
* Guard pages: unmapped page below each kernel stack and each user stack.

## 6. Page-table ownership and address spaces

* Every page-table page is a frame owned by exactly one address space (or the
  kernel master tables). Owner records are implicit in the table tree; destroy
  walks the user half, frees mapped data frames the process owns plus table
  frames, then the PML4.
* New address space: allocate a PML4, copy the upper-half 256 entries from the
  kernel master PML4 (those entries point to **shared** kernel page-table
  subtrees, never freed by process destruction), zero the lower half
  (**private**).
* Kernel mappings added after address spaces exist must go in pre-populated
  top-level slots so already-created spaces see them (the kernel heap/stack
  regions' PML4 entries are allocated at boot for this reason).
* TLB: `invlpg` for single-page changes in the active space; `CR3` reload on
  space switch (PCID is deferred). SMP would need shootdown IPIs.

## 7. Kernel and user stacks

| | Kernel stack | User stack |
|---|---|---|
| Size | 16 KiB | 1 MiB reserve, lazily committed (initially all committed) |
| Guard | 1 unmapped page below | 1 unmapped page below |
| Owner | thread | thread (memory belongs to process address space) |
| Switch | `TSS.RSP0` on interrupt; explicit on syscall | user `RSP` saved in trap frame |
| M1 boot stack | 16 KiB `.bss` array, no guard page (known limitation) | — |

## 8. Kernel heap (planned, M3)

* Interface: `kmalloc(size_t)`, `kcalloc(n, size)`, `kfree(void *)`; `krealloc`
  only when a real user appears.
* Semantics: `NULL` on failure or size 0 (returns `NULL`); alignment ≥ 16;
  `kcalloc` rejects `n * size` overflow; `kfree(NULL)` is a no-op; double free
  and invalid pointer detected via header magic in debug builds.
* Implementation: header-based first-fit with split/coalesce over PMM frames
  accessed through the HHDM; moves to the dedicated heap region at M4.
* Tests: allocator core is arch-independent and host-testable (backing store is
  a host buffer); QEMU self-test checks alloc/free sweeps and leak counters.

## 9. User memory safety

See ARCHITECTURE §6 and SECURITY.md: range + overflow checks, `< USER_TOP`
(`0x0000800000000000`), guarded copies, string copy with explicit maximum
length, partial-copy cleanup rules. A pointer being "numerically in range" is
never sufficient; the copy itself is fault-guarded.
