# ADR-0001: Limine bootloader and version-pinning policy

- **Status:** accepted
- **Date:** 2026-10-13

## Context and problem

We need a bootloader that enters x86_64 long mode in the higher half, provides
a memory map, HHDM, framebuffer and RSDP, boots from one ISO on both BIOS and
UEFI firmware, and works in QEMU and VirtualBox without us writing stage-1/2
code.

## Considered alternatives

1. **Limine** — modern, single hybrid BIOS/UEFI ISO, simple protocol, HHDM and
   memory map provided, active upstream. Cons: protocol/API evolves (base
   revisions), third-party binary dependency.
2. **GRUB2 + Multiboot2** — ubiquitous; but entry is 32-bit protected mode, so
   we would write long-mode bring-up, paging setup and a higher-half trampoline
   before `kmain` — more risky early code and more to test.
3. **Custom bootloader / UEFI application** — maximal control, large effort
   unrelated to the OS goals; BIOS+UEFI parity doubles the work.

## Decision

Use **Limine v12.9.3**, Limine boot protocol **base revision 6**, with pinning:

* Release tag `v12.9.3` (commit `e3aa138d383ca4fe061f05931b5a54497cd502f2`).
* Binary release asset `limine-binary.tar.gz`, SHA-256
  `9f42fe9ea2e84d71056529969b1d5c24ba9a2287ffcc01bcce58b0ef8aa98b1d`, downloaded
  at build time by `scripts/fetch-limine.sh` into `build/downloads/` and
  verified before use. No bootloader binaries are committed.
* Protocol header `limine.h` vendored from `limine-protocol` commit
  `3a0526b700e356f0eac1b71a77697b3fd1c707a3` (0BSD) in `third_party/limine/`
  with provenance and SHA-256 recorded in its README.
* Never track a branch (`trunk`, `v*.x-binary`) — they move.

Updating Limine is a deliberate change: new tag, new checksum, re-run all boot
tests, update BOOT.md and COMPATIBILITY.md in the same commit.

## Rationale

Limine removes the highest-risk early assembly while keeping us in control of
everything from `_start` onward. Pinning by checksum gives reproducibility and
supply-chain integrity.

## Trade-offs and consequences

* Build needs network on first use (or a pre-seeded `build/downloads/`).
* The protocol header is trusted third-party code (0BSD).
* The kernel must tolerate bootloader-chosen details (HHDM base, memory-map
  layout) rather than assuming them.
* The Limine tarball's `limine` host tool is compiled from `limine.c` with the
  host C compiler at ISO build time.

## Testing and verification implications

QEMU BIOS and UEFI smoke tests prove the chain for this pin. VirtualBox is
manual. `fetch-limine.sh` rejection of a wrong checksum is tested manually in
the initial report.

## Conditions that could justify revisiting

A protocol change we need; a security advisory; Limine dropping base revision
6; unresolvable VirtualBox incompatibility.
