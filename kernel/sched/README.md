# Kernel Scheduler (`kernel/sched/`)

## Purpose
Preemptive thread scheduling, ready queues, blocking queues, and context dispatch:
- Priority-based or round-robin queue management.
- Timer preemption hooks (`sched_tick`).
- Voluntary yielding (`sched_yield`) and thread blocking (`sched_block`).
- Idle thread execution when no tasks are runnable.

## Planned Milestones
- **M6 (Timer and Scheduling):** Preemptive single-CPU scheduler and idle loop.

## Contracts & Invariants
- Architecture-independent scheduler logic must not manipulate CPU registers directly; all context switches delegate to `arch_context_switch()`.
- Runaway threads must be preempted by periodic timer interrupts.
