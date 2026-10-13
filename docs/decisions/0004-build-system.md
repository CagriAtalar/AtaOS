# ADR-0004: Build system and directory structure

- **Status:** accepted
- **Date:** 2026-10-13

## Context and problem

Need an understandable, scriptable, reproducible build driven from WSL (and
Linux/macOS with the right tools), with outputs isolated from sources.

## Considered alternatives

1. **GNU Make** (+ small shell/Python helpers) — universal, simple, auto
   dependency via `-MMD`.
2. **CMake + Ninja** — good dependency handling; more indirection; extra tool.
3. **Meson / Bazel** — heavier than needed.

## Decision

GNU Make 4.x. `Makefile` (targets), `toolchain.mk` (compiler/flags/checks).
Everything generated goes under `build/` (`build/<debug|release>[-test]/`,
`build/downloads/`, `build/limine/`, `build/logs/`). `distclean` removes
`build/` entirely; `clean` removes the current configuration.

Build modes: `make MODE=debug` (default: `-O0 -g -DATAOS_DEBUG`, `-Werror`) and
`MODE=release` (`-O2 -g` — symbols kept in the ELF — `-DNDEBUG`). The kernel
ELF in `build/<cfg>/ataos.elf` is never stripped; the ISO contains the same
file (size is not a goal yet).

Dependency files (`-MMD -MP`) give automatic header tracking. Build identity
(`version`, mode, git revision or `unknown`) is generated into
`build/<cfg>/generated/buildinfo.h` by a Make rule that tolerates missing git.

Directory layout is as in README; every placeholder directory contains a
README with its purpose and milestone. Host-side tools are bash and Python 3.

Make runs under bash on Linux/WSL; missing prerequisites are detected by
`scripts/env-check.sh` (also invoked via `make env-check`) and the targets that
need them have a prerequisite check that prints the tool name, why it is needed
and an install hint before failing.

## Rationale

Make is the lowest-complexity option that satisfies the requirements; all
non-trivial logic lives in testable scripts.

## Trade-offs and consequences

Make cannot build on native Windows shells without WSL/MSYS (documented).
Recursive-free single Makefile may need splitting into `kernel.mk` fragments
as the tree grows.

## Testing and verification implications

`make test-host`, `make test-qemu`, `make repro-check` (two clean builds, compare
ELF checksums), `make help` lists targets.

## Conditions that could justify revisiting

Build time becoming painful, need for out-of-tree user-space builds with
different flags that Make expresses poorly.
