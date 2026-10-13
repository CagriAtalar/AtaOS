# ADR-0015: Git commit convention and validation tooling

- **Status:** accepted
- **Date:** 2026-10-13

## Context and problem

A consistent history is needed for a long project driven by many short agent
sessions, enforced by tools rather than memory.

## Considered alternatives

1. **Custom `type(scope): description` convention** with a bash validator, test
   suite, optional local hook and CI check.
2. Conventional Commits verbatim (`feat:` allowed without scope, `fix`) —
   project requires a mandatory scope and `bug` instead of `fix`.
3. Free-form messages — no.

## Decision

Format: `type(scope): description`.

* Types: `feat bug test doc refactor build ci chore`.
* Scope required; initial allowed scopes: `boot arch cpu mm proc sched syscall fs
  vfs drivers serial lib log panic user abi toolchain build test docs ci repo`.
  New scopes are introduced by editing `scripts/commit-scopes.txt` and
  `docs/CODING_STYLE.md` in the same commit with a justification in the commit
  body.
* Description: lowercase English imperative start, non-empty, no trailing period,
  header ≤ 72 characters. The first character must be a lowercase letter.
* Validator: `scripts/check-commit-msg.sh <file>`; tests in
  `tests/scripts/test-commit-msg.sh` include the documented invalid examples.
* Hooks: `scripts/install-git-hooks.sh` installs a repo-local `commit-msg` hook
  only when run explicitly (`make hooks`); it never edits global config.
* CI validates the commit messages in a push/PR range.
* Merge and `Revert`/`fixup!` auto-generated headers are accepted by the
  validator only for `Merge ` and `Revert "` prefixes (documented exception).

## Rationale

Mechanical enforcement avoids agent drift; `bug` vs `fix` and mandatory scope
follow the project owner's rules.

## Trade-offs and consequences

Bash validator must run where git hooks run (Git for Windows ships `sh`; the
hook uses `#!/bin/sh`-compatible invocation of `bash`).

## Testing and verification implications

`make test-scripts` runs validator tests (valid and invalid examples).

## Conditions that could justify revisiting

Adoption of changelog automation or a different team convention.
