#!/usr/bin/env bash
# check-deps.sh - mechanically enforce the dependency rules of
# docs/ARCHITECTURE.md section 4 that can be checked with grep:
#   1. no inline asm / port-I/O outside arch/
#   3. user/ must not include kernel-private headers
#   4. kernel code must not include hosted libc headers
# Exit status 1 if any violation is found.
set -u
cd "$(dirname "$0")/.."

fail=0
report() { echo "violation: $*" >&2; fail=1; }

# Rule 1: asm / asm volatile / __asm__ outside arch/ (and outside third_party).
while IFS= read -r hit; do
    report "inline assembly outside arch/: $hit"
done < <(grep -rnE '(^|[^A-Za-z_])(__asm__|asm)[[:space:]]*(volatile|__volatile__)?[[:space:]]*\(' \
            kernel include --include='*.c' --include='*.h' 2>/dev/null || true)

# Rule 4: hosted libc headers in kernel/, include/, arch/.
hosted='stdio\.h|stdlib\.h|string\.h|math\.h|unistd\.h|errno\.h|time\.h|signal\.h|pthread\.h|assert\.h|ctype\.h|setjmp\.h|fcntl\.h|sys/'
while IFS= read -r hit; do
    # Allow our own "kernel/string.h" (quoted, with the kernel/ prefix).
    case "$hit" in *'"kernel/'*|*'"arch/'*) continue ;; esac
    report "hosted libc header in kernel tree: $hit"
done < <(grep -rnE "#[[:space:]]*include[[:space:]]*<($hosted)" kernel include arch \
            --include='*.c' --include='*.h' --include='*.S' 2>/dev/null || true)

# Rule 3: user/ must not include kernel-private headers.
if [ -d user ]; then
    while IFS= read -r hit; do
        report "user code includes kernel-private header: $hit"
    done < <(grep -rnE '#[[:space:]]*include[[:space:]]*[<"](kernel/|arch/)' user 2>/dev/null || true)
fi

if [ "$fail" -eq 0 ]; then
    echo "check-deps: OK"
fi
exit "$fail"
