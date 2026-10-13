# ADR-0010: Syscall entry/return mechanism and ABI

- **Status:** proposed (finalised before M5 implementation starts)
- **Date:** 2026-10-13

## Context and problem

Choose how Ring 3 enters the kernel and the register ABI. Full detail:
[SYSCALL_ABI.md](../SYSCALL_ABI.md).

## Considered alternatives

1. **`SYSCALL`/`SYSRET`** with safe fallback to `IRETQ` for suspicious returns.
2. **`INT 0x80`** with `IRETQ` only — simplest, slower, uses TSS stack switch.
3. **`SYSENTER`/`SYSEXIT`** — not valid for 64-bit on AMD; rejected.

## Decision

`SYSCALL`/`SYSRET` primary; `RAX`=number, args `RDI RSI RDX R10 R8 R9`, return
`RAX` negative errno; kernel clobbers `RAX RCX R11`; `SYSRET` only for canonical
user `RIP`, otherwise `IRETQ`. No user-visible `int 0x80` initially. ABI
versioned and unstable until M5 completes.

## Rationale

Matches common practice, easiest libc porting; the safe-return rule closes the
known non-canonical-`RCX` hazard.

## Trade-offs and consequences

More MSR/GS setup than the interrupt gate; more care with stack switching.

## Testing and verification implications

M5 tests: each syscall; bad pointers; non-canonical return attempt (forged
frame) takes the `IRETQ` path; register preservation.

## Conditions that could justify revisiting

`SYSCALL` misbehaviour in VirtualBox; need for a fast path or `vDSO`.
