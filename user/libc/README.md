# User Minimal C Library (`user/libc/`)

## Purpose
Minimal freestanding/hosted standard C library for user-space programs:
- C runtime startup stub (`crt0.S`).
- System call assembly wrappers (`syscall1`, `syscall2`, etc.).
- Standard I/O wrappers (`printf`, `getchar`, `putchar`).
- String and memory functions (`string.h`).
- User heap allocator (`malloc`, `free`) backed by `SYS_mmap` or `SYS_brk`.

## Planned Milestones
- **M9 (User-Space Environment):** User runtime startup and basic standard library.

## Contracts & Invariants
- Must never link against kernel objects or include kernel private headers.
- Must only include `include/abi/` for syscall numbers and kernel data structures.
