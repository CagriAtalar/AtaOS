# User Init Process (`user/init/`)

## Purpose
The first user-space process spawned by the kernel after initialization:
- PID 1 execution in Ring 3.
- Initialization of basic environment and mounting.
- Spawning of the interactive user shell (`user/shell/`).
- Adoption and reaping of orphaned zombie processes.

## Planned Milestones
- **M9 (User-Space Environment):** Initial `init` program and bootstrap sequence.

## Contracts & Invariants
- Runs strictly in Ring 3 unprivileged mode.
- Interacts with kernel exclusively via `syscall` instructions.
