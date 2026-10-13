# Kernel Device Drivers (`kernel/drivers/`)

## Purpose
Architecture-independent hardware driver abstractions and generic driver interfaces:
- Character device interfaces (consoles, input devices).
- Block device interfaces (disk storage, ramdisks).
- Driver registration and lifecycle hooks.

## Planned Milestones
- **M1 (Current):** Basic UART COM1 serial output (located under `arch/x86_64/cpu/`).
- **M6:** Programmable Interval Timer (PIT) / APIC Timer driver integration.
- **M8:** RAM disk block device driver.
- **M10+:** PS/2 Keyboard, IDE/AHCI disk controllers.

## Contracts & Invariants
- Drivers must interact with kernel core only through defined VFS, interrupt, and memory interfaces.
- Drivers must never arbitrarily modify scheduler or process tables directly.
