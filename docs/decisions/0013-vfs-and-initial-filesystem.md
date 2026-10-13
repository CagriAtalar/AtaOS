# ADR-0013: VFS interface and initial filesystem strategy

- **Status:** accepted (design; M8)
- **Date:** 2026-10-13

## Context and problem

Need file access for user programs without a disk driver, with a VFS that does
not bake in one filesystem. Detail: [FILESYSTEM.md](../FILESYSTEM.md).

## Considered alternatives

1. **initrd (tar/cpio) loaded as a Limine module, exposed as read-only ramfs**.
2. **Writable ramfs only** — no source of programs at boot.
3. **FAT32/ext2 on a virtual disk** — needs a block driver that differs between
   QEMU and VirtualBox.

## Decision

VFS with vnodes, open-file descriptions, per-process fd table and cwd; initial
filesystem **initrd-backed ramfs** (read-only first; writable in-memory nodes
added after). Persistent storage is post-60-day.

## Rationale

No driver work; same behaviour on all emulators; supplies ELF binaries for M7–M9.

## Trade-offs and consequences

No persistence; initrd must be built into the ISO by the build system.

## Testing and verification implications

Host tests for the archive parser and path resolver; QEMU tests for open/read
and EBADF behaviour; per-process cwd independence.

## Conditions that could justify revisiting

Need for persistence (then a block driver + FS ADR).
