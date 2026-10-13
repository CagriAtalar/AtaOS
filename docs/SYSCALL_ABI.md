# System Call ABI

**Status: DRAFT — `PLANNED` (M5). Nothing here is implemented.** The draft
becomes final (and the version number below is fixed) before Ring 3 is
implemented. Source of truth for numbers/errno once implemented:
`include/abi/syscall.h` and `include/abi/errno.h`; this document must match.

ABI version: `0` (draft; unstable). Stability policy: while version is `0`
anything may change. Once M5 ships, numbers are never reused; removed calls
return `-ENOSYS`; new calls get new numbers; struct layouts only grow by
appending with a size/`version` field passed by the caller.

## 1. Entry mechanism (ADR-0010)

Evaluated:

| Option | Pros | Cons |
|---|---|---|
| `SYSCALL/SYSRET` | Fast; standard on x86_64; matches Linux-like libc ports | Needs MSRs (`EFER.SCE`, `STAR`, `LSTAR`, `FMASK`); does not switch stacks; `SYSRET` with a non-canonical `RCX` faults in Ring 0 on Intel (privilege-escalation class) |
| `INT 0x80` / `IRETQ` | Uses normal interrupt path; stack switch by TSS; no MSR setup; always safe return | Slower; gate/IDT dependency |

**Decision (initial):** implement `SYSCALL`/`SYSRET`, but return to user with
`SYSRET` **only** if `RCX` (user RIP) is canonical and `< USER_TOP` and `R11`
(RFLAGS) is sane; otherwise exit through the `IRETQ` path. An `INT 0x80`-style
path is not provided to user code. If the `SYSCALL` path proves problematic on
VirtualBox, the `IRETQ` path is the documented fallback.

Required CPU state before the first return to Ring 3:

* `EFER.SCE=1`; `STAR[47:32]`=kernel CS, `STAR[63:48]`= base for user segments
  (selector layout: kernel CS, kernel SS, user SS−8…, user CS per SYSRET rules);
  `LSTAR` = entry; `FMASK` clears `IF`, `DF`, `TF`, `AC`.
* GDT has user data and user code descriptors in the order SYSRET requires.
* `TSS.RSP0` = current thread kernel stack top.
* `KERNEL_GS_BASE` MSR holds the `struct percpu *`; entry does `swapgs`, saves
  user `RSP` into percpu scratch, loads the kernel stack, pushes the trap
  frame; exit restores and `swapgs`.
* Interrupts are disabled from `SYSCALL` entry until the stack is switched;
  re-enabled afterwards (handlers are sleepable).
* Faults during entry/exit run on the IST/kernel stack and treat the process as
  killed; never return to user with kernel `GS`/`CR3` state.

## 2. Calling convention

| Item | Convention |
|---|---|
| Syscall number | `RAX` |
| Arguments 1–6 | `RDI`, `RSI`, `RDX`, `R10`, `R8`, `R9` |
| Return value | `RAX` (≥ 0 success value; −4095..−1 is `-errno`) |
| Clobbered by kernel | `RAX`, `RCX`, `R11` only; all other registers preserved |
| Error convention | Negative `errno`; the libc wrapper may translate to `errno`/−1 |
| Direction flag | Must be clear on entry |
| Numbering | Dense, starting at 0; stable once released |
| Struct layout | Naturally aligned, fixed-width types only, explicit padding, no pointers inside structs initially |

## 3. Initial syscall set

| # | Name | Signature | Behaviour |
|---|---|---|---|
| 0 | `write` | `ssize_t write(int fd, const void *buf, size_t len)` | `fd` 1 and 2 → serial/log console until the VFS exists (M8). `len == 0` returns 0 without touching `buf`. Returns bytes written or `-EFAULT`, `-EBADF`, `-EINVAL` |
| 1 | `exit` | `noreturn void exit(int status)` | Terminates the process, status kept for `wait` later |
| 2 | `yield` | `int yield(void)` | Voluntary reschedule, returns 0 (M6) |
| 3 | `getpid` | `pid_t getpid(void)` | Returns current process id |

Anything else returns `-ENOSYS`. Later milestones add `read/open/close`
(M8), `spawn/wait` (M7–M9). `spawn` (create + load in one step) is
preferred to a full `fork` initially (ADR-0012): fewer states, no COW needed.

## 4. Errno values (draft; Linux-compatible numbers)

`EPERM=1 ENOENT=2 EIO=5 EBADF=9 ENOMEM=12 EFAULT=14 EINVAL=22 EMFILE=24
ENOSYS=38`. Compatibility with Linux numbers is chosen to ease libc porting;
it does not imply Linux syscall compatibility.

## 5. User-pointer validation

For each syscall taking a user pointer:

| Rule | Behaviour |
|---|---|
| Which args are user pointers | Marked in the table in `include/abi/syscall.h` comments (`write`: `buf`) |
| Zero length | Valid, returns without validating the pointer (documented per call) |
| Overflow | `ptr + len` computed with overflow check; wrap → `-EFAULT` |
| Range | `ptr + len <= USER_TOP`; otherwise `-EFAULT` |
| Cross-page | Allowed; each page is touched through the guarded copy; an unmapped page → `-EFAULT` |
| Copy semantics | Data is copied into a kernel buffer (`copy_from_user`) before use; the kernel never re-reads user memory after validation (TOCTOU) |
| Failure | `-EFAULT`; no partial side effects beyond bytes reported as written; kernel buffers freed |
| Strings | `strncpy_from_user` with caller-provided max; no terminator within max → `-ENAMETOOLONG`-like `-EINVAL` until a better errno is needed |

Handlers stay small: validation + call into the owning subsystem.
