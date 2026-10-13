# User/Kernel ABI Definitions (`include/abi/`)

## Purpose
Public, stable definitions shared between the kernel and user-space:
- System call numbers (`syscall_nums.h`).
- Syscall error codes and errno values (`errno.h`).
- Common data structures passed across the user/kernel boundary (`stat.h`, `time.h`).

## Planned Milestones
- **M5 (Ring 3 and System Calls):** Initial system call numbers and error definitions.

## Contracts & Invariants
- Headers in `include/abi/` are the ONLY kernel headers permissible for inclusion by user-space applications and user libc.
- Must remain stable across kernel revisions; no kernel-private internal data structures may be defined here.
