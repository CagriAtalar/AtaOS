#!/usr/bin/env bash
# AtaOS lint and static analysis wrapper (QR-007).

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

echo "=== AtaOS Lint & Static Analysis ==="

echo "1. Checking module dependencies..."
bash "${ROOT_DIR}/scripts/check-deps.sh"

echo "2. Checking shell script syntax (bash -n)..."
for sh_file in $(find "${ROOT_DIR}/scripts" "${ROOT_DIR}/tests" -name "*.sh"); do
    bash -n "$sh_file"
done
echo "All shell scripts passed syntax check."

echo "3. Optional static analysis tools:"
if command -v shellcheck >/dev/null 2>&1; then
    echo "Running shellcheck..."
    shellcheck "${ROOT_DIR}/scripts"/*.sh
else
    echo "  shellcheck: NOT RUN (not installed)"
fi

if command -v cppcheck >/dev/null 2>&1; then
    echo "Running cppcheck..."
    cppcheck --enable=all --suppress=missingIncludeSystem -I "${ROOT_DIR}/include" "${ROOT_DIR}/kernel"
else
    echo "  cppcheck: NOT RUN (not installed)"
fi

echo "Lint completed successfully."
exit 0
