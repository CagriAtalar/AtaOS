#!/usr/bin/env bash
# AtaOS commit message validator (ADR-0015).
# Usage: ./scripts/check-commit-msg.sh <path-to-commit-msg-file>
# Or: echo "feat(boot): add entry" | ./scripts/check-commit-msg.sh -

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SCOPES_FILE="${SCRIPT_DIR}/commit-scopes.txt"

if [ $# -lt 1 ]; then
    echo "Usage: $0 <commit-msg-file>" >&2
    exit 2
fi

MSG_FILE="$1"
if [ "$MSG_FILE" = "-" ]; then
    HEADER="$(head -n 1)"
else
    if [ ! -f "$MSG_FILE" ]; then
        echo "error: file not found: $MSG_FILE" >&2
        exit 2
    fi
    # Strip comments (# ...)
    HEADER="$(grep -v '^#' "$MSG_FILE" | grep -v '^[[:space:]]*$' | head -n 1 || true)"
fi

if [ -z "$HEADER" ]; then
    echo "error: commit message is empty" >&2
    exit 1
fi

# 1. Header length <= 72
HEADER_LEN=${#HEADER}
if [ "$HEADER_LEN" -gt 72 ]; then
    echo "error: commit header exceeds 72 characters ($HEADER_LEN chars):" >&2
    echo "  '$HEADER'" >&2
    exit 1
fi

# 2. Conventional Commit regex: type(scope): description
# Types: feat, bug, test, doc, refactor, build, ci, chore
REGEX='^(feat|bug|test|doc|refactor|build|ci|chore)\(([a-z0-9_-]+)\): ([a-z0-9].*)$'

if [[ ! "$HEADER" =~ $REGEX ]]; then
    echo "error: commit message does not match required format:" >&2
    echo "  Expected: type(scope): short description" >&2
    echo "  Allowed types: feat, bug, test, doc, refactor, build, ci, chore" >&2
    echo "  Got: '$HEADER'" >&2
    exit 1
fi

TYPE="${BASH_REMATCH[1]}"
SCOPE="${BASH_REMATCH[2]}"
DESC="${BASH_REMATCH[3]}"

# 3. Scope validation
if [ -f "$SCOPES_FILE" ]; then
    if ! grep -q "^${SCOPE}$" "$SCOPES_FILE"; then
        echo "error: unknown scope '$SCOPE'." >&2
        echo "  Allowed scopes are listed in scripts/commit-scopes.txt:" >&2
        echo "  $(tr '\n' ' ' < "$SCOPES_FILE")" >&2
        exit 1
    fi
fi

# 4. Description checks:
# - No trailing period
if [[ "$DESC" =~ \.$ ]]; then
    echo "error: description must not end with a period:" >&2
    echo "  '$DESC'" >&2
    exit 1
fi

# - Starts with lowercase letter or digit
FIRST_CHAR="${DESC:0:1}"
if [[ "$FIRST_CHAR" =~ [A-Z] ]]; then
    echo "error: description must start with a lowercase letter:" >&2
    echo "  '$DESC'" >&2
    exit 1
fi

exit 0
