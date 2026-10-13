# x86_64 Interrupt and Exception Handling (`arch/x86_64/interrupt/`)

## Purpose
Hardware interrupt, CPU exception, and interrupt controller logic for x86_64:
- Interrupt Descriptor Table (IDT) configuration.
- Common ISR entry and exit assembly stubs (`isr_common_stub`).
- CPU exception handlers (Page Fault `#PF`, General Protection `#GP`, Double Fault `#DF`).
- PIC / IO-APIC routing and End-of-Interrupt (EOI) signaling.

## Planned Milestones
- **M2 (CPU Fundamentals):** IDT initialization, 32 exception handlers, and fault diagnostics.
- **M6 (Timer and Scheduling):** Hardware timer IRQ handling and preemption.

## Contracts & Invariants
- Handlers must preserve all general-purpose registers before calling C handlers.
- Double Fault `#DF` and NMI must use dedicated Interrupt Stack Table (IST) stacks in TSS to prevent triple faults on kernel stack overflow.
