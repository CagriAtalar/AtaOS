# Filesystem and VFS Design

Status: **`PLANNED`** (M8; ELF loading in M7 may read from the initrd earlier).
No code exists.

## 1. Object model

```
process.fdtable[fd] --> open_file (description) --> vnode --> fs-private node
        int                offset, flags, refs      identity     (ramfs entry)
```

| Object | Identity | Lifetime | Notes |
|---|---|---|---|
| File descriptor | small integer, per process | Until `close` or process exit | Never equal to an inode; indexes the table |
| Open-file description | kernel object | Refcount = number of fds (+ internal) pointing at it | Holds offset and access mode; shared by `dup`/inherit |
| vnode | `(filesystem, node id)` | Refcount; cached | The VFS's view of a file/directory |
| Filesystem node | fs-private | Owned by the filesystem | Opaque to VFS |

Descriptor tables, `cwd`, and root belong to the **process**, never globals.

## 2. VFS interface (indicative)

```c
struct vfs_ops {            /* provided by each filesystem */
    int  (*lookup)(struct vnode *dir, const char *name, size_t len, struct vnode **out);
    ssize_t (*read)(struct vnode *, uint64_t off, void *buf, size_t len);
    ssize_t (*write)(struct vnode *, uint64_t off, const void *buf, size_t len);
    int  (*readdir)(struct vnode *, uint64_t *cookie, struct dirent_k *out);
    int  (*stat)(struct vnode *, struct stat_k *out);
    void (*release)(struct vnode *);   /* last reference dropped */
};
```

The VFS core never names a concrete filesystem; mounting registers
`(path vnode) → (filesystem root vnode)`.

## 3. Path resolution

* Components are split on `/`; `.` and `..` handled in the VFS; maximum path
  length 4096 and component length 255 (bounded copies from user space).
* Absolute paths start at the process root (initially the global root);
  relative paths start at `process->cwd`.
* Mount crossing: when a resolved vnode is a mount point, continue at the
  mounted root; `..` at a mounted root goes back across the mount.
* Symlinks are not supported initially (`-EINVAL`/`-ENOENT`).
* Every lookup bounds-checks lengths and rejects embedded NULs.

## 4. Error conventions

Negative errno (`-ENOENT`, `-ENOTDIR`, `-EBADF`, `-EINVAL`, `-EMFILE`,
`-EIO`, `-ENOMEM`). A closed or never-opened fd yields `-EBADF`; operations on
the wrong type yield `-EINVAL`/`-ENOTDIR`.

## 5. Concurrency and lifetime

Single CPU initially: the `fs` lock (IRQ-safe spinlock, short) protects the
mount table and refcounts; filesystem reads that may block do so without
holding it. References: `vnode_get/put`, `open_file_get/put`; the final `put`
calls the owner's `release`. Closing an fd while another thread is in a call
using it relies on the open-file refcount, not on the fd slot.

## 6. First filesystem

**initrd/ramfs from a Limine module** (tar/cpio archive parsed once into
in-memory nodes), read-only first, writable ramfs second. Rationale and
alternatives in ADR-0013: no disk driver required, runs identically on QEMU and
VirtualBox, makes ELF programs available (M7/M8).

## 7. Persistent storage (post-60-day)

Block-device interface (`read_blocks/write_blocks`), then a simple filesystem
(FAT32 or a custom log). Driver choice is deferred (AHCI vs virtio-blk vs NVMe;
VirtualBox default AHCI/PIIX vs QEMU virtio) — see COMPATIBILITY.md.

## 8. Resource-cleanup rules

Opening: allocate description → allocate fd slot → install; failure at any
step releases earlier ones. Process exit closes all fds (each `put`). Failed
`spawn` before publish leaves no fd table entries behind.
