# ADR-0012: ELF64 subset and initial execution model

- **Status:** accepted (design; M7)
- **Date:** 2026-10-13

## Context and problem

Choose the user executable format and the process-creation API.

## Considered alternatives

1. **Standard ELF64, static, non-PIE `ET_EXEC`, subset** — standard toolchains,
   inspectable with `readelf`.
2. **Custom flat binary format** — trivial loader; locks us into custom tooling.
3. **Dynamic/PIE ELF** — needs relocations/interpreter; deferred.

Process creation: **`spawn(path, argv)`** (create + load) vs **`fork`+`exec`**.

## Decision

* Accept: ELF64, little-endian, `ET_EXEC`, `EM_X86_64`, `PT_LOAD` only (others
  ignored except `PT_GNU_STACK` for NX stack confirmation), 4 KiB-aligned
  segments, no interpreter (`PT_INTERP` rejected), no TLS, no dynamic section.
* Validation as listed in SECURITY.md §3; zero `p_memsz - p_filesz` (BSS);
  entry must be inside an executable `PT_LOAD`; maximum segments (e.g. 8) and
  total mapped size capped.
* Stack: SysV-style initial stack with `argc`, `argv[]`, NULL, empty `envp`,
  16-byte aligned.
* Creation: **`spawn`** first. `fork` needs COW or full copy and a more complex
  lifecycle; deferred, with `spawn` semantics making a later `fork` additive.
* Failure at any step unwinds (PROCESS_MODEL §2).

## Rationale

Standard toolchain output lets us develop user programs with unmodified clang;
`spawn` minimises states.

## Trade-offs and consequences

Ported POSIX software expecting `fork` will need adaptation or later work.

## Testing and verification implications

Host-testable validator (pure function over a byte buffer) with fixtures:
truncated header, wrong class, huge `p_memsz`, overlapping segments,
`p_offset` overflow, entry outside text, W+X segment; QEMU tests load a valid
program.

## Conditions that could justify revisiting

Need for PIE/shared libraries; Doom/libc ports requiring `fork`.
