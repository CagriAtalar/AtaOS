#!/usr/bin/env bash
# Test suite for scripts/check-commit-msg.sh (ADR-0015).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CHECKER="${SCRIPT_DIR}/../../scripts/check-commit-msg.sh"

echo "=== Testing Commit Message Validator ==="

pass_count=0
fail_count=0

test_valid() {
    local msg="$1"
    if echo "$msg" | bash "$CHECKER" - >/dev/null 2>&1; then
        pass_count=$((pass_count + 1))
    else
        echo "FAIL: Expected valid message to pass: '$msg'" >&2
        fail_count=$((fail_count + 1))
    fi
}

test_invalid() {
    local msg="$1"
    if echo "$msg" | bash "$CHECKER" - >/dev/null 2>&1; then
        echo "FAIL: Expected invalid message to be rejected: '$msg'" >&2
        fail_count=$((fail_count + 1))
    else
        pass_count=$((pass_count + 1))
    fi
}

# Valid examples:
test_valid "feat(boot): add Limine x86_64 boot support"
test_valid "feat(mm): define physical allocator interface"
test_valid "bug(mm): fix page alignment"
test_valid "test(proc): cover process cleanup"
test_valid "doc(arch): document virtual memory layout"
test_valid "refactor(arch): isolate CPU-specific code"
test_valid "build(toolchain): validate cross-compiler"
test_valid "ci(repo): add kernel smoke-test workflow"
test_valid "chore(repo): update ignore rules"

# Invalid examples:
test_invalid "feat(): add stuff"                      # empty scope
test_invalid "feat: add memory manager"                # missing scope
test_invalid "Update files"                            # not conventional commit
test_invalid "bug(mm): Fix allocator."                 # capital letter and trailing period
test_invalid "feat(mm): fix allocator."                # trailing period
test_invalid "feat(unknownscope): add allocator"       # invalid scope
test_invalid "feat(boot): "                            # empty description
test_invalid "feat(boot): this commit header description is excessively long and definitely exceeds the seventy-two character limit" # > 72 chars

echo "Commit message validator tests: $pass_count passed, $fail_count failed"
if [ "$fail_count" -gt 0 ]; then
    exit 1
fi
exit 0
