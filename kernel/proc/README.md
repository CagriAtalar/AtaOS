# Kernel Process Management (`kernel/proc/`)

## Purpose
Architecture-independent process and thread data structures, lifecycle state transitions, and process isolation:
- Process Control Block (PCB) and Thread Control Block (TCB).
- Parent-child process trees, PID management, and exit code collection.
- Per-process file descriptor table and memory space references.

## Planned Milestones
- **M6 (Timer and Scheduling):** Thread model and execution state.
- **M7 (ELF64 and Process Lifecycle):** Process spawning, address space construction, and cleanup.

## Contracts & Invariants
- A process owns an address space and file table; a thread owns an execution context and stack.
- Process cleanup must guarantee zero resource leaks upon termination.
