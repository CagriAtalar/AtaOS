# Process and Thread Model

Status: **`PLANNED`** (M5–M7). No code exists. Interfaces are indicative;
exact signatures are fixed when each milestone starts.

## 1. Objects

### Process (`struct process`)

| Field | Meaning | Ownership / lifetime |
|---|---|---|
| `pid` | Unique id, never reused while the process (or a zombie of it) exists | Allocated at creation |
| `state` | `NEW, ACTIVE, ZOMBIE, REAPED` | Changed only under `proc` lock |
| `aspace` | Address space (`struct aspace *`) | Owned; destroyed when last thread exits and the process is reaped |
| `parent`, `children` | Parent link (pid, not pointer, to avoid dangling) | Optional until `spawn/wait` (M7+) |
| `fdtable` | Per-process descriptor table | Owned; closed on exit |
| `cwd` | Referenced vnode | Reference held; dropped on exit |
| `exit_status` | Valid in `ZOMBIE` | Read once by `wait` |
| `refcount` | Holders: threads, parent's child list entry | Last put frees the struct |
| `threads` | List of threads | Each thread holds a ref |

### Thread (`struct thread`)

| Field | Meaning | Ownership |
|---|---|---|
| `tid` | Unique id | — |
| `state` | `NEW, READY, RUNNING, BLOCKED, DEAD` | Scheduler lock |
| `context` | Saved callee-saved regs + `RSP` (arch-defined opaque) | Thread |
| `kstack` | Base/size of 16 KiB kernel stack + guard | Thread; freed after the thread is off-CPU |
| `ustack` | User stack mapping (user threads) | Process address space |
| `proc` | Owning process (non-NULL for user threads; NULL for kernel threads) | Reference |
| `wait` | Wait-queue link and wake reason | Scheduler |
| `timeslice` | Remaining ticks | Scheduler |

Process ≠ thread: scheduling operates on threads; the process is the resource
container. The scheduler must not assume one thread per process
(`proc->threads` is a list from day one even if length is always 1 initially).

## 2. Lifecycle

```
 create_process   NEW --(load ok, publish)--> ACTIVE
                       \--(any failure)--> unwind, free, never visible

 thread:  NEW -> READY -> RUNNING -> (timer) READY
                              \-> BLOCKED --(wake)--> READY
                              \-> DEAD (exit or kill)

 process: ACTIVE --(last thread dead)--> ZOMBIE --(wait/auto-reap)--> REAPED -> freed
```

Creation order (all-or-nothing, `spawn`-style):

1. Allocate `struct process` and pid.
2. Create the address space (PML4 with shared kernel half).
3. Load the executable (ELF64 subset) into it.
4. Create the initial user stack and push `argv`.
5. Create the initial thread (kernel stack, context set to the user-entry trampoline).
6. Create the fd table (inherit nothing in M7; stdio in M9).
7. **Publish** (add to process list and run queue) only after all steps succeeded.

Any failure unwinds in reverse order. A partially built process must never be
reachable from the scheduler or the pid table.

Termination: `exit` or a fatal user fault marks the thread DEAD; the process
closes fds and drops cwd; the address space is destroyed after no thread of it
is running (never while it is the active `CR3`: switch to the kernel space
first); status stored; parent notified; zombie reaped by `wait` (or immediately
for orphan/init-adopted).

A thread cannot free its own kernel stack while running on it; a DEAD thread is
freed by the next thread run or by a reaper (design chosen at M6 and recorded in
ADR-0011's follow-up).

## 3. Scheduler (ADR-0011)

Alternatives considered: round-robin; static priority; MLFQ/CFS-style.
Chosen: **single-queue round-robin, 10 ms quantum**, because it is trivial to
reason about, provides a testable no-starvation property, and is replaceable
behind `sched_pick_next()`.

| Aspect | Design |
|---|---|
| Queues | One FIFO `ready` queue; blocked threads live on the wait queue they sleep on; DEAD list for reaping |
| Idle thread | Always READY-eligible, lowest preference, loops `sti; hlt` |
| Preemption | Timer IRQ decrements timeslice; at 0 sets `need_resched`; switch occurs at IRQ exit |
| Context switch | `arch_context_switch(&old->ctx, new->ctx)` saves `rbx, rbp, r12–r15, RSP, RIP`; floating-point state (fxsave) handled by arch per ADR-0011 |
| Address space switch | If `old->proc->aspace != new->proc->aspace`, load new `CR3` before switching stacks (kernel half identical so kernel code stays mapped) |
| Block/wake | `sched_block(waitq)` / `sched_wake(waitq)`; both require the sched lock with IRQs off |
| Invariants | (1) exactly one thread is `RUNNING`; (2) a thread is on at most one queue; (3) the running thread is not on `ready`; (4) `need_resched` is only consumed at safe points (IRQ exit / syscall exit); (5) `sched` lock is the outermost lock |
| Runaway test | Two infinite-loop user programs each bump a counter via `write` or a shared page; the test requires both counters to be non-zero after N ticks. A third test spawns a `while(1);` and verifies a short program still exits |

## 4. Synchronisation (single CPU)

See ARCHITECTURE §7.3. `sched`, `proc`, and `fs` locks are IRQ-safe spinlocks
taken in that order. Sleeping locks (mutex) are introduced only if a concrete
need appears.
