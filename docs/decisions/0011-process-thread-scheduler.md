# ADR-0011: Process/thread model and initial scheduler policy

- **Status:** accepted (design; M5–M6)
- **Date:** 2026-10-13

## Context and problem

Define process vs thread, and the first scheduler. Detail:
[PROCESS_MODEL.md](../PROCESS_MODEL.md).

## Considered alternatives

1. **Process = 1 thread** (merge objects) — less code; blocks threads later.
2. **Process (resource container) + threads (execution)** from day one, even if
   each process has one thread initially.
3. **Policy:** round-robin vs static priorities vs MLFQ/CFS.

## Decision

Separate `process` and `thread` structures. Single-CPU, one FIFO ready queue,
round-robin, 10 ms quantum, idle thread. FPU/SSE: kernel never uses it; user
state saved with `fxsave` on context switch (eager) at M6 — simplest, correct,
revisit for lazy switching only if profiling demands. Kernel preemption off while
spinlocks are held; the kernel is otherwise preemptible only at documented
points (IRQ exit when returning to user, or to a preemptible kernel thread).

## Rationale

Splitting the objects later is painful; round-robin gives the testable
no-starvation property with minimal code.

## Trade-offs and consequences

No priorities or fairness tuning; eager FPU save costs cycles.

## Testing and verification implications

Runaway-process test (two CPU-bound programs both progress); resource
reclamation test after exit; context-switch register preservation test.

## Conditions that could justify revisiting

Interactive latency problems; SMP; real-time needs.
