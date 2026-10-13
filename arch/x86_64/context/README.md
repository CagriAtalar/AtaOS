# x86_64 CPU Context Switching (`arch/x86_64/context/`)

## Purpose
Hardware-specific context switching and execution state preservation:
- Callee-saved register preservation (`R12-R15`, `RBX`, `RBP`, `RSP`).
- Low-level assembly context switch (`arch_context_switch`).
- Kernel stack pointer switching and TSS `rsp0` update.
- CR3 (PML4) address-space switching when moving between different processes.

## Planned Milestones
- **M6 (Timer and Scheduling):** Assembly context switch routine and stack initialization.

## Contracts & Invariants
- Context switch routines must maintain 16-byte stack alignment required by the System V AMD64 ABI.
- Must update the Task State Segment (TSS) Ring 0 stack pointer (`rsp0`) on every context switch to ensure syscalls/interrupts from user space have a valid kernel stack.
