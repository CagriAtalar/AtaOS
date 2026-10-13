# Boot

Status: M1 boot path `IMPLEMENTED`; test status in the
[initial report](reports/initial-bootstrap.md). Everything after "boot complete"
is `PLANNED`.

## 1. Chain

```
Power on
  -> Firmware (BIOS  | UEFI)
  -> Limine v12.9.3 (BIOS: limine-bios-cd.bin + limine-bios.sys | UEFI: BOOTX64.EFI)
  -> reads /boot/limine/limine.conf, loads /boot/ataos.elf (Limine protocol)
  -> kernel entry `_start` (arch/x86_64/boot/entry.S)
  -> kmain() (kernel/core/main.c)
```

## 2. Bootloader pin (ADR-0001)

| Item | Value |
|---|---|
| Limine release | `v12.9.3` (tag object `ea3cc04b5f7b2278c511e8ea566d4b10a1569123`, commit `e3aa138d383ca4fe061f05931b5a54497cd502f2`) |
| Binary tarball | `limine-binary.tar.gz` from the GitHub release |
| SHA-256 | `9f42fe9ea2e84d71056529969b1d5c24ba9a2287ffcc01bcce58b0ef8aa98b1d` (matches the digest published by GitHub for the asset **and** the value computed locally on download) |
| Protocol header | `limine.h` from `limine-protocol` commit `3a0526b700e356f0eac1b71a77697b3fd1c707a3`, SHA-256 `0aadf2633c85d8cb145344c968add158aeb1deb68af567d55ba8431c492b6d4c`, vendored in `third_party/limine/` (0BSD) |
| Protocol base revision requested | 6 (bootloader source defines `SUPPORTED_BASE_REVISION 6`) |

`scripts/fetch-limine.sh` downloads the tarball into `build/downloads/`,
verifies the checksum **before** extraction, and refuses to proceed on mismatch.

## 3. ISO layout

```
iso_root/
  boot/ataos.elf                 kernel (the unstripped build/<cfg>/ataos.elf is kept separately)
  boot/limine/limine.conf
  boot/limine/limine-bios.sys
  boot/limine/limine-bios-cd.bin
  boot/limine/limine-uefi-cd.bin
  EFI/BOOT/BOOTX64.EFI
```

Created with `xorriso -as mkisofs ... --efi-boot` then `limine bios-install`
(exact command in `scripts/make-iso.sh`). One hybrid ISO serves BIOS and UEFI,
QEMU and VirtualBox. Reproducibility notes: `SOURCE_DATE_EPOCH` is passed to
xorriso where supported; QR-001 is checked on `ataos.elf` (the ISO volume
timestamps are not claimed reproducible; see report).

## 4. Requests made by the kernel

| Request | Why | If missing |
|---|---|---|
| Base revision 6 | Defines HHDM/memmap behaviour | `PANIC` (cannot be reported via boot info; serial still works) |
| Bootloader info | Identification in log | Warn, continue |
| Firmware type | Log + later ACPI/UEFI decisions | Warn, continue |
| HHDM | Phys→virt direct map | `PANIC` (required) |
| Memory map | PMM input | `PANIC` (required) |
| Framebuffer | Presence/metadata only (no drawing in M1) | Log "none", continue |
| RSDP | Later ACPI/APIC discovery | Log "none", continue |
| Executable address | Log kernel phys/virt base | Warn, continue |
| Paging mode (4-level) | Pin to 4-level paging | Warn, continue |

Limine's start/end request markers delimit the request area
(`.limine_requests`). All requests are `volatile`; the compiler is not allowed to
assume responses are NULL.

## 5. Machine state at entry (summary)

Per PROTOCOL.md (x86-64): long mode, paging enabled with bootloader tables,
interrupts disabled, bootloader GDT loaded, **no valid IDT for the kernel**. The
kernel immediately switches to its own 16 KiB `.bss` stack (the entry stack is
bootloader-owned memory of unspecified lifetime) and does not enable interrupts
in M1. Direction flag clear per SysV.

## 6. Ownership and lifetime

* Response structures and memory-map arrays live in
  *bootloader-reclaimable* memory.
* `kernel/core/limine_boot.c` copies what it needs into `struct bootinfo`
  (scalars and a fixed-size memory-map array) and the rest of the kernel only
  sees `bootinfo.h`. Strings (bootloader name/version) are copied into bounded
  buffers.
* Framebuffer: address, dimensions, pitch, bpp are copied. The framebuffer
  memory itself is owned by the display driver later (reserved; never PMM-owned).
* Which allocations must stay reserved: kernel image
  (`EXECUTABLE_AND_MODULES`), framebuffer, ACPI NVS/reclaimable until parsed,
  and bootloader-reclaimable until the retirement point defined in
  MEMORY_LAYOUT §3. Limine's page tables are bootloader-reclaimable, so the
  kernel **must** switch to its own tables (M4) before retiring that memory.

## 7. Failure policy

Missing required responses `PANIC` with a message naming the request. The
panic path uses only the serial port, so it works before anything else exists.

## 8. Expected early output (format, M1)

```
[ataos] AtaOS 0.1.0 (debug, git <rev>) x86_64
[ataos] bootloader: Limine 12.9.3
[ataos] firmware: UEFI64 | BIOS
[ataos] hhdm base: 0xffff800000000000
[ataos] kernel: phys 0x... virt 0xffffffff80000000
[ataos] memory map: <N> entries
[ataos]   0x0000000000000000 - 0x... usable ...
[ataos] memory: usable <X> KiB, reserved <Y> KiB
[ataos] framebuffer: <W>x<H>x<bpp> pitch <P> | none
[ataos] rsdp: 0x... | none
[ataos] boot complete
```

The exact strings are asserted by `tests/qemu/run.py`; changing them requires
updating that runner. HHDM base may differ if the bootloader chooses another.
Unverified: VirtualBox output.
