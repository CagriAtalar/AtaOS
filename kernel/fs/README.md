# Kernel Virtual Filesystem (`kernel/fs/`)

## Purpose
Architecture-independent Virtual Filesystem (VFS) and filesystem implementations:
- VFS node (`vnode`) interface and operations (`vnode_ops_t`).
- Path traversal and lookup (`vfs_lookup`).
- Open file description table and reference counting.
- Initial in-memory filesystem (`ramfs`) / Limine initrd mounting.

## Planned Milestones
- **M8 (VFS and Initial Filesystem):** VFS core, ramfs, and basic file operations (`open`, `read`, `write`, `close`).

## Contracts & Invariants
- File descriptors are per-process integers indexing process file tables; they must not be confused with underlying global vnodes.
- VFS must remain completely independent of disk hardware drivers.
