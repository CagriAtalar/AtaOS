# ADR-0008: Interrupt architecture and interrupt-controller strategy

- **Status:** accepted (design; M2 exceptions, M6 hardware IRQs)
- **Date:** 2026-10-13

## Context and problem

Decide how exceptions and device interrupts are entered, dispatched and
acknowledged, and which controller is used, on both QEMU and VirtualBox.

## Considered alternatives

1. **Local APIC (+ I/O APIC when needed), legacy PIC masked** — modern default,
   needed for SMP later; requires ACPI MADT parsing for I/O APIC (RSDP is
   already requested).
2. **Legacy 8259 PIC only** — trivial; works everywhere in VMs; no SMP path; two
   controllers to remap; 15 IRQ lines.
3. **Start with the PIC then migrate** — double work for little gain.

## Decision

* Exceptions (vectors 0–31): uniform stub → `struct trap_frame` → common C
  dispatcher (M2).
* Hardware IRQs: remap and **mask the 8259 PIC** at boot (to stop spurious
  legacy interrupts), use the **Local APIC** timer (M6) and I/O APIC for
  keyboard when needed (M after M9). Until then no hardware interrupts are
  enabled. If APIC bring-up fails on an environment, the PIC+PIT path is the
  documented fallback (logged).
* Vector map: 0–31 exceptions, 32–47 reserved (legacy range, masked), 48+ APIC
  devices, 0xFF spurious.
* Interrupts stay disabled in M1. First `sti` happens in M6 after the scheduler
  and timer are ready, never earlier than needed.
* Dispatch table of `void (*handler)(struct irq_frame *)`; handlers obey ARCHITECTURE §7.3.
* IST stacks for `#DF`, NMI, `#MC`.

## Rationale

APIC is the long-term path; masking the PIC avoids spurious IRQs; deferring
enablement keeps M1–M5 simple and deterministic.

## Trade-offs and consequences

Needs MSR/MMIO access for the APIC (mapped via HHDM or a dedicated mapping,
uncacheable — requires VMM support, hence M4 before M6).

## Testing and verification implications

M2: fault-injection tests per vector. M6: timer tick counter test; compare
QEMU vs VirtualBox calibration logs.

## Conditions that could justify revisiting

APIC unavailable on a target VM; MSI/MSI-X needs; x2APIC requirement.
