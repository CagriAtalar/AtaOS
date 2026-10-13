# ADR-0002: C standard, assembly policy, and freestanding environment

- **Status:** accepted
- **Date:** 2026-10-13

## Context and problem

Define the implementation language rules for kernel code.

## Considered alternatives

1. **C11 with GNU extensions (`-std=gnu11`)** — widely supported by clang/gcc;
   attributes (`packed`, `aligned`, `noreturn`, `format`) and
   `__builtin_*_overflow` are needed in kernel code.
2. **Strict ISO C17 (`-std=c17`)** — portable but would force `#pragma`/macro
   workarounds for features we want.
3. **Rust/C++** — rejected: toolchain complexity and scope; project prescribes C.

## Decision

* Kernel and user runtime: **C, `-std=gnu11`**, freestanding.
* Only compiler-provided headers: `stddef.h stdint.h stdbool.h stdarg.h
  stdalign.h limits.h`. No hosted libc.
* Assembly only where C cannot express the operation: entry stubs, interrupt
  stubs, context switch, syscall entry, specific instructions (`lgdt`, `lidt`,
  `wrmsr`...) as small inline-asm helpers confined to `arch/`. Assembly files
  use `.S` (preprocessed, AT&T syntax as assembled by clang's integrated
  assembler); each routine has a header comment describing register contract
  and stack usage.
* Fixed-width types for hardware/ABI data; `size_t` for sizes; `uintptr_t` for
  address arithmetic; no `long`-based assumptions beyond LP64 (`-m64`).
* `memcpy/memmove/memset/memcmp` are provided by `kernel/lib` because the
  compiler may emit calls to them.
* No VLAs, no `alloca`, no recursion in kernel code without a documented depth
  bound, no floating point (kernel is built `-mno-sse -mno-80387`).

## Rationale

Predictable ABI and code generation; minimal surface where portable
behaviour might vary.

## Trade-offs and consequences

GNU extensions tie us to clang/gcc (acceptable; ADR-0003). `-mno-sse` means no
vectorised `memcpy` and any accidental FP use fails at compile time.

## Testing and verification implications

Warnings enabled (`-Wall -Wextra -Wpedantic`-adjacent set, see toolchain.mk);
`scripts/check-deps.sh` scans for hosted-header includes; link with `-nostdlib`
proves no libc symbols leak in.

## Conditions that could justify revisiting

Need for C23 features, or FPU/SIMD use in the kernel (would require save/restore
policy and a separate ADR).
