# ADR-0009: Timer and timekeeping strategy

- **Status:** accepted (design; M6)
- **Date:** 2026-10-13

## Context and problem

Pick the scheduler tick source and monotonic time base that behave acceptably in
QEMU (TCG/KVM) and VirtualBox.

## Considered alternatives

1. **Local APIC timer, calibrated** — per-CPU, scales to SMP; needs calibration
   against another source.
2. **PIT (8254)** — universal and fixed 1.193182 MHz; slow I/O port access;
   global, single channel shared; fine as fallback/calibrator.
3. **HPET** — precise, but requires ACPI table parsing and is not guaranteed
   present in VirtualBox; deferred.
4. **TSC / TSC-deadline** — fast and precise but frequency discovery and
   invariance differ across VMs; Limine can supply `tsc_frequency` when known.

## Decision

* Tick: **Local APIC timer** in periodic (or one-shot re-armed) mode at 100 Hz
  (10 ms quantum), calibrated against the PIT channel 2/0 over ~10–50 ms at
  boot; the calibration source and result are logged.
* Monotonic clock: nanoseconds since boot from the TSC when invariant TSC is
  reported and calibrated, else from tick counts.
* Fallback: PIT as tick source if APIC timer calibration is implausible.
* HPET and TSC-deadline are explicitly deferred.
* EOI after handler; spurious ticks ignored. Sleep = timer queue sorted by
  expiry, woken from the tick path via `sched_wake`.

## Rationale

APIC timer matches the interrupt strategy; PIT availability on both emulators
gives a safe fallback and calibration base.

## Trade-offs and consequences

Calibration noise under emulation; a time source differing from wall time.
Timekeeping accuracy is not a goal in the first 60 days.

## Testing and verification implications

Log calibration; test that N ticks arrive within a loose bound under TCG; run
manual VirtualBox check; never assert exact tick rates.

## Conditions that could justify revisiting

Poor calibration on VirtualBox; need for tickless idle; SMP.
