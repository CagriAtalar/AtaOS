# ADR-0003: Compiler, linker, and cross-compilation strategy

- **Status:** accepted
- **Date:** 2026-10-13

## Context and problem

Observed environment (WSL2 Ubuntu 24.04): `clang 18.1.3`, `ld.lld 18.1.3`,
`gcc 13.3.0` (native, no `x86_64-elf-gcc`), GNU binutils 2.42, GNU Make 4.3.
Installing a dedicated cross-GCC would require building from source or system
packages we are not permitted to install without approval.

## Considered alternatives

1. **Clang + LLD, `--target=x86_64-unknown-none-elf`** — a single toolchain is
   already a cross-compiler for every target; no sysroot; reproducible across
   hosts; available now.
2. **`x86_64-elf-gcc` cross-compiler** — the traditional route; not installed;
   building takes tens of minutes and adds a toolchain-build step.
3. **Native GCC with `-ffreestanding`** — works but leaks host defaults
   (PIE, stack protector, host include dirs, CET) that must all be disabled;
   fragile across distributions.

## Decision

* `CC = clang --target=x86_64-unknown-none-elf`, `LD = ld.lld`, assembler = clang
  integrated assembler. `toolchain.mk` selects them, can be overridden
  (`make CC=... LD=...`), and `make env-check` verifies version and that a test
  freestanding compile+link works, failing with an actionable message.
* Minimum clang/lld **16** (documented; tested with 18.1.3 only).
* Host tests use the host `cc` (any C11 compiler) with sanitizers.
* Key flags and why (full list in `toolchain.mk`):

| Flag | Reason |
|---|---|
| `-ffreestanding -fno-builtin` (for lib/string.c) | no hosted assumptions; stops loop→`memcpy` recursion |
| `-nostdlib -static` (link) | no libc/startfiles |
| `-mcmodel=kernel` | top-2-GiB link address (ADR-0005) |
| `-mno-red-zone` | interrupts would clobber the red zone below `RSP` |
| `-mno-sse -mno-sse2 -mno-mmx -mno-80387 -mgeneral-regs-only` | no FPU state in kernel (ADR-0011 policy) |
| `-fno-pic -fno-pie`, link `-no-pie`/`-static` | fixed-address, non-relocated image (Limine loads at link address) |
| `-fno-stack-protector -fno-stack-check` | no guard-variable runtime yet; reconsidered at M4 |
| `-fno-omit-frame-pointer -g` | stack walking, GDB |
| `-fno-lto` | simple, debuggable output |
| `-ffunction-sections -fdata-sections` + `--gc-sections` | smaller image; requests kept with `KEEP` |
| `-Wall -Wextra -Wshadow -Wconversion`-subset `-Werror` (debug) | warnings are bugs; not disabled to pass |
| `-ffile-prefix-map=$(CURDIR)=.`, `-Wl,--build-id=none` | reproducible output |
| `-z max-page-size=0x1000 -z noexecstack` | 4 KiB page-aligned segments; no executable stack |

## Rationale

Clang/LLD works now with no installation, gives uniform behaviour, and a
single, small flag set is easier to validate than GCC host-default removal.

## Trade-offs and consequences

* Compiler-rt builtins are not linked; if clang emits calls to e.g. `__udivti3`
  the link fails loudly (acceptable; handle when seen).
* GCC-specific behaviour is untested; supporting GCC later needs a CI matrix.
* LLD requires `--image-base`/`ENTRY` care handled in the linker script.

## Testing and verification implications

`make env-check`, a real link, `readelf` inspection (segments, addresses), real
boot.

## Conditions that could justify revisiting

Need for GCC-only features, a clang regression in codegen, or distro
packaging making `x86_64-elf-gcc` trivial and preferred by the maintainer.
