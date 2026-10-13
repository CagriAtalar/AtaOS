/*
 * serial.c - early COM1 (16550 UART) console.
 *
 * Fixed legacy port 0x3F8, 115200 baud, 8N1, FIFOs enabled. This is a
 * best-effort early console: there is no probe or loopback self-test, because
 * the panic path must keep working even on platforms where the port is absent
 * (writes to a missing port are ignored by the hardware/VM).
 *
 * Transmit waits on LSR.THRE with a bounded spin so that an absent or stuck
 * UART can never hang the kernel.
 */
#include <stdint.h>

#include "arch/io.h"
#include "kernel/arch.h"

#define COM1 0x3F8

#define UART_DATA 0 /* RX/TX, or divisor low when DLAB=1 */
#define UART_IER 1  /* interrupt enable, or divisor high when DLAB=1 */
#define UART_FCR 2
#define UART_LCR 3
#define UART_MCR 4
#define UART_LSR 5

#define LSR_THRE 0x20
#define TX_SPIN_LIMIT 100000

void arch_early_console_init(void)
{
    outb(COM1 + UART_IER, 0x00); /* no UART interrupts */
    outb(COM1 + UART_LCR, 0x80); /* DLAB on */
    outb(COM1 + UART_DATA, 0x01); /* divisor 1 -> 115200 baud */
    outb(COM1 + UART_IER, 0x00);
    outb(COM1 + UART_LCR, 0x03); /* 8 data bits, no parity, 1 stop bit; DLAB off */
    outb(COM1 + UART_FCR, 0xC7); /* enable + clear FIFOs, 14-byte threshold */
    outb(COM1 + UART_MCR, 0x03); /* DTR + RTS, interrupts (OUT2) off */
}

void arch_early_console_putc(char c)
{
    for (unsigned spin = 0; spin < TX_SPIN_LIMIT; spin++) {
        if (inb(COM1 + UART_LSR) & LSR_THRE)
            break;
    }
    outb(COM1 + UART_DATA, (uint8_t)c);
}
