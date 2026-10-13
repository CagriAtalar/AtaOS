#!/usr/bin/env bash
# Install AtaOS repo-local commit-msg hook.
# Opt-in: only runs on explicit invocation (make hooks).

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
HOOK_DEST="${ROOT_DIR}/.git/hooks/commit-msg"

if [ ! -d "${ROOT_DIR}/.git" ]; then
    echo "error: not a git repository root: ${ROOT_DIR}" >&2
    exit 1
fi

mkdir -p "${ROOT_DIR}/.git/hooks"

cat > "${HOOK_DEST}" << 'EOF'
#!/usr/bin/env bash
# AtaOS commit-msg hook
bash scripts/check-commit-msg.sh "$1"
EOF

chmod +x "${HOOK_DEST}"
echo "Successfully installed repo-local commit-msg hook to ${HOOK_DEST}"
