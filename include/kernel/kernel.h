/*
 * kernel.h - entry point of the architecture-independent kernel.
 */
#ifndef ATAOS_KERNEL_KERNEL_H
#define ATAOS_KERNEL_KERNEL_H

#include "kernel/compiler.h"

/*
 * Kernel entry: called once from the architecture boot stub (entry.S) on the
 * boot stack with interrupts disabled. Never returns.
 */
void kmain(void);

#endif /* ATAOS_KERNEL_KERNEL_H */
