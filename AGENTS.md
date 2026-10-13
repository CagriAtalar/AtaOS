# AGENTS.md — Permanent Rules for AI Coding Agents in AtaOS

This document defines the binding operational constraints, architectural invariants, and workflows for all AI coding agents working on the AtaOS repository.

---

## 1. Project Mission

Build a modular, architecturally sound x86_64 operating system from scratch using Limine and freestanding C (`-std=gnu11`). The project targets single-CPU long mode, with eventual Ring 0/Ring 3 separation, virtual memory management, preemptive scheduling, VFS/ramfs, and isolated user programs. Development proceeds in small, verifiable daily milestones over ~60 days.

---

## 2. Required Reading Order

At the start of every session, you **must** read files in this order before modifying or designing code:
1. `AGENTS.md` (this file) — binding invariants and safety rules.
2. `ROADMAP.md` — identify the active milestone and its explicit acceptance criteria.
3. `docs/ARCHITECTURE.md` — system structure, layer boundaries, and dependency directions.
4. Subsystem-specific architectural documents relevant to your task:
   - `docs/MEMORY_LAYOUT.md` for memory / paging / allocator tasks.
   - `docs/BOOT.md` for bootloader / handoff / firmware tasks.
   - `docs/SYSCALL_ABI.md` for syscalls and Ring 3 boundaries.
   - `docs/PROCESS_MODEL.md` for tasks, threads, context switching, scheduling.
   - `docs/FILESYSTEM.md` for VFS, inodes, file descriptors.
   - `docs/SECURITY.md` for trust boundaries, validation, pointer safety.
   - Relevant ADRs in `docs/decisions/`.
5. `git status` and recent commits (`git log -n 5 --oneline`).

---

## 3. Status Vocabulary

When describing requirements, features, or roadmap items, use only these five labels:

| Status | Meaning |
|---|---|
| `IMPLEMENTED` | Code exists in the repository. |
| `TESTED` | Concrete automated or manual test cases have been executed. |
| `VERIFIED` | **All** documented acceptance criteria for the milestone or requirement have passed. |
| `PLANNED` | Architecture/design is documented; implementation has not started. |
| `BLOCKED` | Progress depends on an unresolved dependency or missing environmental tool. |

**Rule:** Never label code `VERIFIED` simply because it compiled or passed a single unit test.

---

## 4. Architectural Invariants

1. **Architecture Before Expansion:** Never implement a subsystem before documenting its contracts and invariants in `docs/` and writing an Architecture Decision Record (ADR) in `docs/decisions/`.
2. **Strict Module Boundary:** Architecture-independent code in `kernel/` must **never** include `arch/x86_64/include/arch/` directly, execute inline assembly, access I/O ports, or manipulate control registers (`CR*`, `MSR`). All low-level CPU access goes through `include/kernel/arch.h`.
3. **No Hosted libc:** Kernel code links with `-nostdlib` and uses only compiler freestanding headers (`stddef.h`, `stdint.h`, `stdbool.h`, `stdarg.h`, `stdalign.h`, `limits.h`) and `kernel/lib/`.
4. **User / Kernel Header Isolation:** User-space code (`user/`) must **never** include kernel headers (`include/kernel/`). It interacts with the kernel exclusively through `include/abi/`.
5. **No Circular Dependencies:** Interface boundaries must be acyclic. Forward-declaration tricks to disguise design cycles are prohibited.
6. **Isolated Build Artifacts:** All build outputs reside under `build/`. No generated files are tracked in Git.

---

## 5. Security & Memory Safety Rules

1. **Principle of Least Privilege:** Pages must never be mapped both writable and executable (`W^X`). User pages must have the user bit set; kernel pages must never be user-accessible.
2. **Never Trust User Pointers:** User pointers passed via system calls must be verified before dereferencing:
   - Check `ptr + size <= USER_TOP` with overflow protection.
   - Access user memory only through guarded copy functions (`copy_from_user`, `copy_to_user`).
3. **Integer Arithmetic Safety:** All length and allocation calculations must use overflow-checked arithmetic (e.g. `__builtin_add_overflow`, `__builtin_mul_overflow`).
4. **Deterministic Failure:** Prefer kernel panic with structured register and file/line diagnostics over silent execution with corrupted kernel state.
5. **Resource Reclamation:** Every resource acquisition must have a deterministic release path. On partial initialization failure, unwind previous allocations in reverse order.

---

## 6. Concurrency & Interrupt-Context Rules

1. **Interrupt Context Constraints:** Handlers executing in interrupt or exception context must **never**:
   - Sleep or block on a synchronization primitive.
   - Allocate memory dynamically via the heap (`kmalloc`).
   - Perform lengthy I/O operations.
   - Invoke scheduling logic directly (except setting `need_resched`).
2. **Spinlock Protocol:** On single-CPU, spinlocks must disable local interrupts (`spin_lock_irqsave`) and restore prior interrupt state (`spin_unlock_irqrestore`). Never issue an unconditional `sti`.
3. **Preemption Restrictions:** Kernel preemption is disabled whenever a spinlock is held.
4. **Hierarchical Lock Ordering:** Acquire locks in documented order: `sched` → `proc` → `fs` → `heap` → `pmm` → `log`.

---

## 7. Testing Requirements

1. **Multi-Tier Testing:**
   - **Host Unit Tests (`tests/host/`):** Pure logic (strings, math, bitmap allocators, ELF parsing) must be compiled with host tools and run with AddressSanitizer and UndefinedBehaviorSanitizer (`-fsanitize=address,undefined`).
   - **QEMU Smoke Tests (`tests/qemu/run.py`):** Kernel and ISO images are booted headless under QEMU; output is captured from serial and evaluated for required boot banners.
   - **Script & Tooling Tests (`tests/scripts/`):** Commit validators, build scripts, and dependency checks must be exercised.
2. **Regression Testing:** Every bug fix must include a corresponding test case where practical.
3. **Truthful Test Reporting:** Never fabricate test results. If an emulator (e.g. VirtualBox or UEFI OVMF) is missing or unverified, explicitly report it as `NOT RUN`.

---

## 8. Commit Conventions

AtaOS enforces conventional commits with required scopes. Commit header format:
```
type(scope): short imperative description
```
- **Allowed Types:** `feat`, `bug`, `test`, `doc`, `refactor`, `build`, `ci`, `chore`.
- **Allowed Scopes:** `boot`, `arch`, `cpu`, `mm`, `proc`, `sched`, `syscall`, `fs`, `vfs`, `drivers`, `serial`, `lib`, `log`, `panic`, `user`, `abi`, `toolchain`, `build`, `test`, docs`, `ci`, `repo`.
- **Format Constraints:**
  - Header must not exceed 72 characters.
  - Description must begin with a lowercase letter, use imperative tense, and have no trailing period.
- **Verification Tool:** Enforced via `scripts/check-commit-msg.sh`.

---

## 9. Session Workflow

Every agent session must follow this exact progression:
1. **Discover Environment:** Check compilers, tools, emulators, and git status.
2. **Identify Target:** Pick the smallest atomic task for the current milestone from `ROADMAP.md`.
3. **Formulate Plan:** Specify file changes, acceptance criteria, and planned tests.
4. **Implement Minimally:** Write code adhering to coding style and architectural contracts.
5. **Run Verification:** Execute host unit tests, dependency checks, and QEMU smoke tests.
6. **Self-Review:** Inspect `git diff` for leaks, unbounded loops, or contract violations.
7. **Document & Report:** Update `ROADMAP.md`, update status in docs, and write the session report.
8. **Draft Commit Message:** Provide verified commit messages according to the rules (do not push directly if instructed).

---

## 10. Scope Control & Handling Blockers

1. **Strict Scope Control:** Do not implement features scheduled for future milestones. E.g., do not implement PMM in M1, do not implement Ring 3 in M3.
2. **Non-Destructive Operations:** Never run `git reset --hard`, `git push --force`, or delete user workspaces.
3. **Package Installation Policy:** Never attempt `apt-get install` or modify global configurations without explicit user instruction. If a tool is missing, report it as missing, outline the install instructions for the user, and continue with all tasks that do not depend on it.

---

## 11. Common Coding-Agent Failure Modes to Avoid

- **Premature Implementation:** Writing paging or scheduler code before documenting memory layouts or process models.
- **Hallucinated Test Results:** Claiming that a QEMU boot test passed without actually launching the emulator and parsing serial output.
- **Leaking Host Assumptions:** Assuming hosted glibc headers or Linux kernel headers exist in the freestanding build.
- **Overwriting User Work:** Modifying user files or configuration without inspecting diffs first.
- **Commit Format Drift:** Writing messages like `Update main.c` or `feat: add kernel` (missing required scope or wrong case).

---

## 12. Required End-of-Session Report

Every session must generate or update a report in `docs/reports/` and output a summary containing:
1. Environment findings and tool versions.
2. Architecture summary and ADRs written or updated.
3. Files modified/created in the repository.
4. Exact build, run, and test commands executed.
5. Concrete test results table (`PASS`, `FAIL`, `NOT RUN`).
6. Actual serial log captured during emulator execution.
7. Formatted commit messages for verification.
8. Current milestone status and the next 3 recommended tasks.
