# Kernel System Call Subsystem (`kernel/syscall/`)

## Purpose
System call dispatch, user pointer validation, argument copying, and error translation:
- Dispatch table mapping syscall numbers to handler routines.
- User pointer security bounds checking (`validate_user_buffer`, `copy_from_user`, `copy_to_user`).
- Return code normalization.

## Planned Milestones
- **M5 (Ring 3 and System Calls):** Initial syscall set (`SYS_exit`, `SYS_write`, `SYS_yield`, `SYS_getpid`).

## Contracts & Invariants
- Untrusted user pointers must never be dereferenced directly by kernel handlers without prior range validation and copy routines.
- Syscall handlers must never cause unhandled kernel page faults or panics due to malformed user input.
