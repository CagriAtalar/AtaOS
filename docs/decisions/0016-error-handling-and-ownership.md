# ADR-0016: Error-handling and resource-ownership policy

- **Status:** accepted
- **Date:** 2026-10-13

## Context and problem

Consistent error conventions and ownership rules prevent leaks and
use-after-free as the kernel grows.

## Considered alternatives

1. **Negative errno return (`int`), `NULL` for pointers** — simple, ABI-aligned.
2. `Result`-style structs — verbose in C.
3. Out-parameter + `bool` — loses the error cause.

## Decision

* Status-returning functions: `0` success, `-E*` failure (values from
  `include/abi/errno.h` once it exists). Pointer-returning allocators return
  `NULL`. Fallible APIs are marked `WARN_UNUSED`.
* Classes and responses are in ARCHITECTURE §12.
* `ASSERT(cond)` for internal invariants (panics, includes file:line); never used
  to validate user input; `PANIC(fmt, ...)` for unrecoverable states.
* Ownership: each resource has one creator-owner; transfers are explicit
  ("takes ownership" in the function contract); shared objects are
  reference-counted; cleanup uses reverse-order `goto` labels; pointers are
  cleared after free; debug heap poisons freed memory.
* Function contracts (comment above each non-trivial function): parameters,
  ownership, locking, failure behaviour, context (IRQ/sleepable).

## Rationale

Matches kernel conventions; errors are cheap and visible.

## Trade-offs and consequences

Discipline required; no automatic cleanup in C.

## Testing and verification implications

Counters (frames, heap bytes, fd slots) must return to baseline after failed
operations; tests inject allocation failures once a failure-injection hook
exists.

## Conditions that could justify revisiting

Introducing richer error context; adopting `cleanup` attribute helpers.

## Amendment (M4, 2026-10-20): errno values and leaf ownership

* **G1:** `include/kernel/errno.h` is the kernel-internal errno header for M4.
  It uses Linux-compatible numeric values only for the errors currently used
  (`EINVAL`, `ENOENT`, `ENOMEM`, `EEXIST`); functions return `0` or a negative
  value. The public user ABI header remains `include/abi/errno.h` and must be
  kept numerically compatible when M5 fixes it.
* **G4:** page-table frames are owned by the address space and freed by
  `aspace_destroy()`. Leaf data frames mapped through `aspace_map()` remain
  caller-owned in M4; the M5 process teardown takes ownership of releasing them
  before destroying the space. `aspace_destroy()` is never called while its
  PML4 is the active `CR3`.
* Fallible table allocation in the VMM unwinds the tables it installed; a failed
  large-range map also removes all translations it added. Full rollback of
  table frames allocated by an *earlier* failed attempt is not claimed and is
  tracked as an M5 hardening item.
