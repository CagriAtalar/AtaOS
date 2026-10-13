# Contributing to AtaOS

Thank you for contributing to AtaOS. This project is built incrementally with high architectural discipline and automated verification.

---

## 1. Development Principles

Before contributing, please read:
- [AGENTS.md](file:///c:/Users/cagri/OneDrive/Desktop/kuiz/gitHUB/cagOS/AtaOS/AtaOS/AGENTS.md) — Core invariants and failure avoidance guidelines.
- [ARCHITECTURE.md](file:///c:/Users/cagri/OneDrive/Desktop/kuiz/gitHUB/cagOS/AtaOS/AtaOS/docs/ARCHITECTURE.md) — System boundaries and module contracts.
- [ROADMAP.md](file:///c:/Users/cagri/OneDrive/Desktop/kuiz/gitHUB/cagOS/AtaOS/AtaOS/ROADMAP.md) — Phased milestones M1 through M12.

---

## 2. Environment Setup

### Required Tools:
- **Build:** GNU Make, Clang 18+ and LLD 18+ (or cross-compiler).
- **ISO Creation:** `xorriso`, `mtools`.
- **Testing:** Python 3, `qemu-system-x86_64` (v8.2+), `ovmf` (UEFI).
- **Git:** Git 2.40+.

Run environment check:
```bash
make env-check
```

---

## 3. Workflow and Rules

1. **One Logical Change per Commit:** Avoid monolithic commits mixing unrelated changes.
2. **Follow Conventional Commits (ADR-0015):**
   - Format: `type(scope): concise description`
   - Allowed types: `feat`, `bug`, `test`, `doc`, `refactor`, `build`, `ci`, `chore`.
   - Valid scopes: `boot`, `arch`, `cpu`, `mm`, `proc`, `sched`, `syscall`, `fs`, `vfs`, `drivers`, `serial`, `lib`, `log`, `panic`, `user`, `abi`, `toolchain`, `build`, `test`, `docs`, `ci`, `repo`.
   - Length: Header <= 72 characters, lowercase description, no trailing period.
   - Install local hook: `make hooks`.
3. **Verify Before Submitting:**
   ```bash
   make test-host       # Host unit tests with ASan/UBSan
   make test-scripts    # Commit validator & script tests
   make test-qemu       # Headless QEMU BIOS/UEFI boot test
   make repro-check     # Bit-for-bit reproducible build test
   ```
4. **No Destructive Operations:** Never force-push or amend public commits.
