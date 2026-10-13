# AtaOS Coding Style and Conventions

This document establishes the C and Assembly style guidelines for the AtaOS codebase.

---

## 1. C Standard and Formatting

1. **Standard:** C11 with GNU extensions (`-std=gnu11`).
2. **Freestanding:** The kernel compiles with `-ffreestanding -nostdlib`. Only `<stddef.h>`, `<stdint.h>`, `<stdbool.h>`, and `<stdarg.h>` from the compiler runtime are permitted.
3. **Indentation:** 4 spaces. No tabs.
4. **Line Length:** Maximum 100 characters per line (80 for commit messages).
5. **Brace Placement:** K&R style: opening brace on the same line for control structures and functions; closing brace on its own line.
   ```c
   void kmain(void) {
       if (condition) {
           do_something();
       } else {
           do_other();
       }
   }
   ```
6. **Tooling:** Run `make format` to format code using `.clang-format`. Run `make format-check` to verify in CI.

---

## 2. Naming Conventions

1. **Functions:** Lowercase with underscores (`snake_case`). Subsystem prefixes are required for non-static functions:
   - `serial_init()`, `serial_putchar()`
   - `klog_info()`, `panic()`
   - `pmm_alloc_frame()`, `vmm_map_page()`
2. **Types:** Typedefs end with `_t` (e.g., `uint64_t`, `paddr_t`, `vaddr_t`, `process_t`). Structs have meaningful names (e.g., `struct limine_memmap_entry`).
3. **Constants and Macros:** Uppercase with underscores (`CAPITAL_SNAKE_CASE`):
   - `ARCH_PAGE_SIZE`, `SERIAL_PORT_COM1`, `ALIGN_UP()`
4. **Variables:** Descriptive `snake_case`. Short names (`i`, `p`, `len`) are permitted only for small local scopes.
5. **Private / Static Symbols:** Must be declared `static`.

---

## 3. Types and Portability

- Use fixed-width integer types from `<stdint.h>`: `uint8_t`, `uint16_t`, `uint32_t`, `uint64_t`, `int32_t`, `int64_t`.
- Use `size_t` for sizes and counts.
- Never use plain `long` or `unsigned long` when fixed width is required across architectures.
- Use explicit physical address types (`uintptr_t` or future `paddr_t`) and virtual address types (`uintptr_t` or future `vaddr_t`).
- Use `bool`, `true`, and `false` from `<stdbool.h>`.

---

## 4. Header Files and Inclusions

1. **Header Guards:** Use `#ifndef ATAOS_<PATH>_<NAME>_H` `#define ATAOS_<PATH>_<NAME>_H` `#endif`.
2. **Inclusion Order:**
   - Standard compiler freestanding headers (`<stddef.h>`, `<stdint.h>`, `<stdbool.h>`).
   - Core kernel headers (`<kernel/compiler.h>`, `<kernel/log.h>`).
   - Architecture headers (`<arch/io.h>`).
   - Local module headers.
3. **Boundary Rules (ADR-0004):**
   - Architecture-independent code (`kernel/`) must never directly include internal `arch/` private headers.
   - User-space code must never include kernel-internal headers.

---

## 5. Assertions and Error Handling

1. **Assertions:** Use `ASSERT(condition, message)` for developer invariant checks that should never fail in correct code.
2. **Static Assertions:** Use `STATIC_ASSERT(condition, message)` for compile-time structure size and alignment verification.
3. **Runtime Errors:** Always return explicit error codes (`int` or `err_t` negative errno-style values) for foreseeable runtime conditions.
4. **Panic:** Use `panic("message")` strictly for unrecoverable fatal errors during early boot or catastrophic kernel corruption.
