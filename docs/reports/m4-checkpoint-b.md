# M4 Checkpoint B — Virtual Memory

- **Date:** 2026-10-20
- **Branch:** `m4/virtual-memory` (base `main` remains `9f48173`)
- **Scope:** M4 Phases 1–5. No Ring 3, syscalls, scheduler, or user programs.
- **Overall status:** M4 core `IMPLEMENTED, TESTED`; not labelled `VERIFIED`
  because the Ring 3 negative-access and user-fault-isolation criteria need M5.

## 1. Environment Findings

| Item | Result |
|---|---|
| Host | Windows 11 + WSL2 Ubuntu 24.04 |
| Compiler | Ubuntu clang 18.1.3 |
| Linker | Ubuntu LLD 18.1.3 |
| Build | GNU Make 4.3, GNU bash 5.2.21 |
| ISO | xorriso 1.5.6 |
| Emulator | QEMU 8.2.2 (`qemu-system-x86_64`) |
| Firmware | OVMF `/usr/share/ovmf/OVMF.fd` |
| Host tests | GCC 13.3.0, ASan + UBSan available |
| GDB | 15.1 |
| `clang-format` | **missing → format check NOT RUN** |
| `shellcheck`, `cppcheck`, `clang-tidy` | **missing → optional lint NOT RUN** |
| VirtualBox | **missing → VirtualBox test NOT RUN** |
| CI workflow | file updated; **NOT RUN locally** |

Environment log: `build/logs/checkpoint-env.log`.

## 2. Architecture Summary

- **Master kernel PML4:** upper half copied from Limine with canonical sign
  extension; lower half stays private per address space. Text is RX, rodata R,
  data/bss/stacks/heap RW+NX, no U bit, no W|X.
- **Pre-flight before CR3:** 26 critical mappings checked (code, exception
  stubs/dispatcher, panic/serial/halt paths, boot stack, GDT/IDT/TSS, IST tops,
  HHDM PMM aliases, dynmap absence, and four stack guards).
- **Mapping API:** `vmm_map`/`vmm_unmap`/`vmm_resolve` enforce alignment,
  overflow, USER_TOP, unknown flags, W^X, overlap `-EEXIST` and missing
  `-ENOENT`; fallible table allocation rolls back installed translations.
- **Address spaces:** `aspace_create` copies shared kernel PML4 entries and
  zeroes the private half; `aspace_map` requires `VMM_USER`; `aspace_destroy`
  frees private table frames plus PML4 and leaves leaf data frames caller-owned.
- **Faults:** every documented #PF error bit is decoded; CR2, origin, access,
  cause, fetch kind, reserved/PK/SGX and a W^X/NX/not-present hint are printed.
- **Guards (E2):** dedicated 4 KiB unmapped pages below the boot stack and each
  IST stack, removed before CR3 load and covered by pre-flight.
- **Contracts preserved:** no arch headers in `kernel/mm` beyond `kernel/arch.h`;
  no hosted libc; `include/abi` not used by the kernel; no generated files in
  Git.

## 3. ADRs and Documentation

- `docs/decisions/0007-virtual-memory-policy.md` — amended for guard pages,
  #PF reporting, leaf ownership and malformed large-leaf panics.
- `docs/decisions/0016-error-handling-and-ownership.md` — amended for the
  kernel `errno.h` values, G4 leaf ownership, and partial-rollback limits.
- Updated: `ROADMAP.md`, `docs/ARCHITECTURE.md`, `docs/MEMORY_LAYOUT.md`,
  `docs/PROCESS_MODEL.md`, `docs/SECURITY.md`, `docs/TESTING.md`,
  `docs/DEBUGGING.md`.

## 4. Files Modified or Created

Kernel / arch:
- `kernel/mm/vmm.c`, `kernel/mm/vmm_internal.h`, `kernel/mm/vmm_validate.c`
- `kernel/mm/mm_internal.c`, `kernel/mm/mm_internal.h`
- `kernel/mm/aspace.c`, `kernel/mm/aspace_internal.h`
- `include/kernel/aspace.h`, `include/kernel/errno.h`, `include/kernel/mm_types.h`
- `arch/x86_64/mm/paging_encode.c`, `arch/x86_64/mm/preflight.c`
- `arch/x86_64/interrupt/exception.c`, `arch/x86_64/interrupt/page_fault.c`
- `arch/x86_64/include/arch/page_fault.h`
- `arch/x86_64/boot/entry.S`, `arch/x86_64/cpu/gdt.c`,
  `arch/x86_64/cpu/test_exit.c`, `arch/x86_64/include/arch/gdt.h`
- `kernel/core/main.c`, `include/kernel/arch.h`, `Makefile`, `toolchain.mk`

Tests / tooling:
- `tests/host/test_mm.c`, `tests/host/test_aspace.c`,
  `tests/host/test_pf.c`, `tests/host/main.c`
- `tests/qemu/run.py`
- `.github/workflows/ci.yml`, `.gitignore`

## 5. Commands Executed

```bash
make MODE=debug
make MODE=release
make test-host
make test-scripts
make lint
make test-qemu QEMU_RUN_FLAGS=--require-all
make repro-check
make test
make env-check
```

Logs are under `build/logs/` (e.g. `checkpoint-m4-test.log`,
`e2-host.log`, `e2-test-qemu.log`, `e2-repro-check.log`, `e2-lint.log`,
`checkpoint-env.log`).

`make format-check` was **NOT RUN** because `clang-format` is missing. No
packages were installed.

## 6. Test Results

| Test | Result | Evidence |
|---|---|---|
| Host unit tests (ASan/UBSan) | **PASS 40/40** | `build/logs/e2-host.log`, `checkpoint-m4-test.log` |
| Commit-message validator tests | **PASS 17/17** | `checkpoint-m4-test.log` |
| Limine checksum test | **PASS** | `checkpoint-m4-test.log` |
| Dependency boundary check | **PASS** | `checkpoint-m4-test.log` |
| QEMU BIOS boot | **PASS** (exit 33) | `qemu_bios_boot.log` |
| QEMU UEFI boot | **PASS** (exit 33) | `qemu_uefi_boot.log` |
| Panic handler | **PASS** (exit 35) | `qemu_panic_test.log` |
| #DE, #UD, #GP | **PASS** (exit 35 each) | `qemu_fault_*.log` |
| #PF unmapped | **PASS** (exit 35) | `qemu_fault_pf.log` |
| #PF write-protect / NX | **PASS** (exit 35 each) | `qemu_fault_pfwp.log`, `qemu_fault_pfnx.log` |
| #DF via IST1 | **PASS** (exit 35) | `qemu_fault_df.log` |
| Stack guard overflow | **PASS** (exit 35) | `qemu_fault_stackovf.log` |
| Full QEMU suite | **PASS 11/11, 0 failed, 0 skipped** | `e2-test-qemu.log` |
| Reproducible build | **PASS** (identical SHA-256) | `e2-repro-check.log` |
| Lint (deps + bash -n) | **PASS** | `e2-lint.log` |
| `shellcheck` / `cppcheck` | **NOT RUN** (missing) | `e2-lint.log` |
| `format-check` | **NOT RUN** (clang-format missing) | — |
| VirtualBox | **NOT RUN** (VBoxManage missing) | — |
| GitHub Actions workflow | **NOT RUN** (not pushed/dispatched) | — |

## 7. PMM and VMM Counters (actual boot log)

```text
[ataos] pmm: 255 MiB managed (65503 frames), 260352 KiB free (65088 frames), 1660 KiB reserved (415 frames)
[ataos] vmm: audit clean (1537 leaves: 11 text RX, 3 rodata R, 11 data RW, 1512 other NX; 11 executable, no U bit, no W|X)
[ataos] vmm: pre-flight check passed (26 critical mappings)
[ataos] vmm: master kernel page tables active (CR3 0x000000000007b000, 18 table frames)
[ataos] aspace: self-test passed (create/map/resolve/destroy; PMM used 34 -> 34)
```

The aspace suite returns PMM used frames to their baseline. The QEMU suite
reported `Passed: 11, Failed: 0, Skipped: 0`.

## 8. Defect Table

The IDs below are the audit labels used during M4. Each was re-checked against
the current code before changing it; no finding was "fixed" without a failing
test or an inspected contract.

| ID | Finding | Verification | Resolution |
|---|---|---|---|
| D1 | Large-leaf PAT bit decoded as PS (bit 7/12 confusion) | Host encoding tests | Level-aware PAT/PS decode |
| D2 | Re-protection cleared PAT/other bits | Host encode/decode tests | Preserve all non-permission bits; rewrite W/U/NX only |
| D3 | Translation dropped the in-page offset | Host 4 KiB/2 MiB/1 GiB tests | Return frame base + offset |
| D4 | PML4.PS / malformed non-table entry not handled | Walker code inspection + host tests | Panic with leaf/level/section diagnostics |
| D5 | `vmm_map` accepted invalid/overflowing/overlapping ranges; unmap silently succeeded | Host range tests + QEMU | Alignment, overflow, USER_TOP, flags, `-EEXIST`, `-ENOENT` |
| D6 | Fallible table allocation could panic on OOM | Host failure-injection tests | `mm_alloc_table_try`, unwind installed tables |
| D7 | Failed large-range map left installed translations | Host rollback tests | Remove all translations installed by the failed call |
| D8 | #PF handler printed only the raw vector/error | QEMU #PF tests | Full error-bit decode + CR2/origin/access/cause/hint |
| D9 | No address-space lifecycle existed | Host aspace suite + kernel PMM counters | `aspace_create/destroy/map/unmap/resolve` |
| D10 | Audit did not prove every section class and executable count | Boot audit log | Text RX count equals executable leaves; rodata R; data RW+NX; no U/WX |
| D11 | No in-kernel address-space counter test | Boot self-test | Create/map/resolve/unmap/destroy with `PMM used 34 -> 34` |
| D12 | Fault tests covered only an unmapped #PF | QEMU suite | Added `pfwp`, `pfnx`, `stackovf`; all 11 tests pass |
| D13 | Runner accepted missing images/firmware as suite success | `run.py` inspection | `--require-all`; CI passes the flag |
| D14 | Stale M1/M3 status and mapping comments | Doc/comment review | Commit `2fb7e97` |
| D15 | Trace re-run used one file for serial and `-D` trace | `run_trace` inspection | Separate sinks; `-no-shutdown` documented |
| D16 | mm types/errno scattered | Header review | `include/kernel/mm_types.h`, `include/kernel/errno.h` |
| D17 | Canonical PML4 bases were truncated to 48 bits | Host canonical-base tests | Sign-extend bit 47 |
| D18 | CR3 switched before critical mappings were checked | Boot pre-flight log | 26 specs checked before `arch_mm_set_cr3` |
| D19 | Timeout could leave QEMU/process group running | Runner inspection + timeout path | Process-group kill, serial tail, `FAILED (Timeout)` |
| D20 | Triple-fault trace/no-shutdown workflow undocumented | Docs review | `DEBUGGING.md` §3 + `TESTING.md` |

Known residual: a failed range map removes translations but does not claim full
rollback of table frames allocated by an earlier failed attempt. This is named
as M5 hardening, not silently treated as complete.

## 9. E2 Guard-Page Design

**Implemented this milestone:** boot stack and IST guards.

- Layout: a page-aligned 4 KiB guard immediately below the 16 KiB boot stack,
  and one below each 4 KiB IST stack (IST1 #DF, IST2 NMI, IST3 #MC).
- Boot sequence: Limine maps the guard as part of `.bss`; `vmm_init()` removes
  only the guard PTE from the master PML4 after copying the kernel half and
  before loading CR3. The unmap is a no-op if the bootloader left it absent.
- Pre-flight: all four guards are `MM_EXPECT_UNMAPPED`.
- Audit: guards are subtracted from the writable image page count, so the
  remaining data pages are still proven RW+NX.
- Overflow test: `TEST_FAULT=stackovf` sets RSP to `boot_stack_bottom` and
  pushes. The push hits `CR2=0xffffffff80018ff8`; the #PF frame cannot use the
  guard, so the CPU escalates to #DF on IST1. Observed RSP
  `0xffffffff80019000`, `DF: delivered on IST1`.
- Cost: 16 KiB additional `.bss`, no extra physical frames, four pre-flight
  specs, one test flavour.
- Risk: if the bootloader maps a guard inside a 2 MiB/1 GiB leaf, M4 panics
  rather than splitting unrelated pages. IST1 overflow itself can still
  triple-fault; a second IST would be required to recover from that.
- M5/M6 follow-up: per-thread kernel-stack guard and user-stack guard; switch
  `TSS.RSP0` from the boot stack to the running thread's kernel stack.

## 10. G5 Decisions

- **G1 (errno):** accepted. `include/kernel/errno.h` uses Linux-compatible
  numeric values for the errors currently used; functions return `0` or a
  negative errno. Recorded in ADR-0016.
- **G2 (malformed leaves):** accepted. `copy_table()`/`fix_leaf()` panic with
  leaf range, level and intersecting kernel section; host tests added.
- **G3 (`vmm_resolve`):** accepted. Returns frame base + in-page offset for
  4 KiB, 2 MiB and 1 GiB leaves.
- **G4 (leaf ownership):** accepted. M4 `aspace_destroy()` frees only private
  page tables and the PML4; leaf frames are caller-owned. M5 process teardown
  owns them. Recorded in ADR-0016, MEMORY_LAYOUT §6, PROCESS_MODEL §2 and
  ROADMAP.
- **G5/E1:** deferred; no trivial item remained after D9.
- **G5/E2:** implemented for boot and IST stacks (above). User-stack guard is
  M5 work.
- **G5/E3:** deferred.
- **G5/E4:** deferred to M11; the stale `toolchain.mk` comment was corrected.
- **G6 (address layout):** accepted, no layout changes. Dynmap remains
  `0xffffffffc0000000`–`0xffffffffe0000000`.

## 11. Real Boot Output

BIOS boot, `build/logs/qemu_bios_boot.log` (abridged; full log retained):

```text
[ataos] AtaOS 0.1.0 (debug-test, git 744e30c4ab3d) x86_64
[ataos] gdt: custom GDT and TSS loaded (CS 0x08, DS 0x10, TR 0x28)
[ataos] gdt: tss.rsp0=0xffffffff8001d000 (bootstrap boot stack)
[ataos] gdt: ist1 (#DF)=0xffffffff80012000, ist2 (NMI)=0xffffffff80014000, ist3 (#MC)=0xffffffff80016000
[ataos] cpu: control registers verified (CR0.WP 0x10000, EFER.NXE 0x800)
[ataos] bootloader: Limine 12.9.3
[ataos] firmware: BIOS
[ataos] hhdm base: 0xffff800000000000
[ataos] pmm: 255 MiB managed (65503 frames), 260352 KiB free (65088 frames), 1660 KiB reserved (415 frames)
[ataos] vmm: audit clean (1537 leaves: 11 text RX, 3 rodata R, 11 data RW, 1512 other NX; 11 executable, no U bit, no W|X)
[ataos] vmm: pre-flight check passed (26 critical mappings)
[ataos] vmm: master kernel page tables active (CR3 0x000000000007b000, 18 table frames)
[ataos] vmm: kernel half re-protected (text RX, HHDM/data NX, W^X enforced)
[ataos] vmm: mapping self-test passed (map/resolve/write/unmap round-trip)
[ataos] heap: 63 KiB managed, 63 KiB free
[ataos] heap: self-test passed (split/coalesce and overflow checked)
[ataos] aspace: self-test passed (create/map/resolve/destroy; PMM used 34 -> 34)
[ataos] boot complete
```

Stack-guard overflow, `build/logs/qemu_fault_stackovf.log`:

```text
*** CPU EXCEPTION ***
Vector: 8 (Double Fault (#DF)), Error Code: 0x0
RIP: 0xffffffff800098ce  CS: 0x0008  RFLAGS: 0x0000000000000082
RSP: 0xffffffff80019000  SS: 0x0010  CR2: 0xffffffff80018ff8  CR3: 0x000000000007b000
...
DF: delivered on IST1 (stack or guard-page overflow path)
System halted.
```

## 12. Commit History

| Hash | Message | Verification |
|---|---|---|
| `1d87a45` | `feat(mm): build master kernel page tables with w^x enforcement` | debug/release builds, pre-flight, QEMU |
| `f40cd55` | `test(mm): cover pat, resolve, preflight, and range validation` | host tests |
| `2fb7e97` | `doc(mm): refresh m4 status and mapping contract comments` | review |
| `66f6b51` | `feat(mm): decode page faults and test w^x/nx flavours` | host 38/38, QEMU 10/10 |
| `cb501fd` | `feat(mm): add address-space lifecycle and frame accounting` | host 40/40, QEMU 10/10 |
| `2f2385b` | `feat(mm): guard boot and ist stacks with unmapped pages` | host 40/40, QEMU 11/11 |
| `3aebbd0` | `chore(ci): require all qemu images and ignore pycache` | commit validator, runner direct run |
| `744e30c` | `doc(mm): document m4 status, guards and page-fault diagnostics` | review |
| `d4ef012` | `doc(mm): refresh kernel mm readme status` | review |
| *(this report)* | `doc(mm): add m4 checkpoint b report` | this document |

All messages were validated with `scripts/check-commit-msg.sh`. Nothing was
pushed; `main` was not modified.

## 13. Known Limitations and Risks

- Ring 3 cannot be tested yet; U-bit absence is proven by the kernel-half audit,
  not by a user-mode load. S2/S10 remain partially `PLANNED`.
- User-stack guards, per-thread kernel stacks and `TSS.RSP0` switching are
  M5/M6.
- M4 leaf frames are caller-owned; process teardown must free them.
- IST1 overflow is not recoverable (triple fault).
- `format-check` and optional static analyzers are NOT RUN locally.
- CI workflow is updated but not executed locally; it will run on push/PR.
- VirtualBox is NOT RUN.

## 14. Roadmap Status and Next Tasks

- **Current milestone:** M4 — Virtual Memory Management.
- **Acceptance status:** `TESTED`. All M4 code paths and the three structural
  criteria have passing tests; the user-mode negative test is explicitly M5.
- **Next milestone:** M5 — Ring 3 and syscalls.

Next 3 tasks:

1. **M5 leaf/frame teardown:** free every user leaf frame owned by an address
   space before `aspace_destroy`, with a host test and a PMM-baseline QEMU test.
   Acceptance: repeated process create/exit returns PMM counters to baseline.
2. **M5 Ring 3 boundary test:** map a user page and attempt to read a kernel
   address under CPL3; require #PF with `origin=user`, and prove a second
   process keeps running after the fault. Acceptance: both markers in one QEMU
   log.
3. **M6 per-thread stacks:** allocate 16 KiB kernel stacks with guard pages,
   set `TSS.RSP0` on every context switch, and test stack overflow from a
   non-boot stack. Acceptance: `stackovf` equivalent reaches #DF on IST1 and
   PMM counters stay balanced.
