# User Interactive Shell (`user/shell/`)

## Purpose
Minimal command-line interpreter running in Ring 3:
- Read-Eval-Print Loop (REPL) on serial / terminal console.
- Command line parsing and tokenization.
- Built-in commands (`help`, `clear`, `version`, `exit`).
- Process execution (`fork`/`exec` or `spawn`) for external binaries.

## Planned Milestones
- **M9 (User-Space Environment):** Interactive user shell.

## Contracts & Invariants
- Operates entirely in user mode; commands communicate via standard descriptors 0 (stdin), 1 (stdout), and 2 (stderr).
