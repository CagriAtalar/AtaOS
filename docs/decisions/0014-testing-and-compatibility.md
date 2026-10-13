# ADR-0014: Testing and QEMU/VirtualBox compatibility policy

- **Status:** accepted
- **Date:** 2026-10-13

## Context and problem

Tests must be repeatable, honest, and not depend on one emulator's undocumented
behaviour.

## Considered alternatives

1. **Layered tests**: host unit tests (ASan/UBSan) → headless QEMU smoke tests →
   manual VirtualBox checklist.
2. In-kernel test framework only — cannot test pure logic with sanitizers.
3. VirtualBox automation from day one — requires `VBoxManage` on a host where it
   is not installed; fragile serial capture.

## Decision

Option 1. Details:

* **Host tests**: `tests/host/`, own tiny framework, run under
  `-fsanitize=address,undefined` when the compiler supports it; tests only
  architecture-independent code.
* **QEMU**: `tests/qemu/run.py` boots `-machine q35 -m 256M -display none
  -serial file:<log> -no-reboot` under TCG (no KVM requirement), with a bounded
  timeout, in BIOS and UEFI (OVMF) modes; results: `PASS`, `FAIL`, `TIMEOUT`,
  `ENVERR` (missing tool/firmware: reported `NOT RUN`, never `PASS`).
* **Test-only exit**: QEMU `isa-debug-exit` at I/O port `0xf4`, size 4.
  Writing byte `v` makes QEMU exit with status `(v << 1) | 1`. Convention:
  `0x10` → exit status 33 = test **pass**; `0x11` → 35 = **panic/fail**. Code
  exists only when compiled with `-DATAOS_TEST_BUILD` (`make TEST=1`); the
  default ISO contains no reference to port `0xf4`.
* **VirtualBox**: documented manual procedure plus a conservative helper script
  using the `ataos-` VM-name prefix; it refuses to touch non-prefixed VMs.
* A test that could not run is `NOT RUN`, with a reason, in reports.
* The same kernel image and ISO layout are used for QEMU and VirtualBox; the
  test ISO differs only by the compile-time test-exit hook.

## Rationale

Maximum automated coverage with the tools available in the environment while
refusing to over-claim.

## Trade-offs and consequences

The test ISO and release ISO are different binaries; the plain ISO is boot-checked
by marker only. `isa-debug-exit` is QEMU-specific (acceptable: it is test-only).

## Testing and verification implications

See [TESTING.md](../TESTING.md), [COMPATIBILITY.md](../COMPATIBILITY.md).

## Conditions that could justify revisiting

VirtualBox automation becoming reliable; need for hardware-independent test
exit via serial protocol.
