/*
 * serial.c — multi-port serial driver.
 *
 * Tracks which COM ports have been initialised and provides independent
 * read/write access to each one.  All ports run at 38400 8N1 with
 * 14-byte FIFOs enabled.
 *
 * Port roles are defined in serial.h:
 *   SERIAL_DEBUG (COM1) — kernel debug / KLOG
 *   SERIAL_SHELL (COM2) — serial terminal shell
 *   SERIAL_TERM  (COM3) — terminal emulation
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <kernel/asm.h>
#include <kernel/serial.h>

/* ── Port-state table ────────────────────────────────────────────────────── */

/*
 * The four standard PC COM ports, in the order they appear in the
 * com_ports[] array.  port_ready[i] is 1 once serial_init() has
 * configured com_ports[i].
 */
static const int com_ports[4] = { COM1, COM2, COM3, COM4 };
static int       port_ready[4] = { 0, 0, 0, 0 };

/*
 * port_idx — return the 0-based index of `port` in com_ports[], or -1
 * if `port` is not a recognised COM base address.
 */
static int port_idx(int port)
{
    for (int i = 0; i < 4; i++)
        if (com_ports[i] == port)
            return i;
    return -1;
}

/* ── Low-level line-status helpers ──────────────────────────────────────── */

/* LSR bit 5: transmit-holding register empty (safe to send). */
static inline int tx_empty(int port) { return inb(port + 5) & 0x20; }

/* LSR bit 0: data ready (a byte has been received). */
static inline int rx_ready(int port)  { return inb(port + 5) & 0x01; }

/* ── Public per-port API ─────────────────────────────────────────────────── */

void serial_init(int port)
{
    int idx = port_idx(port);
    if (idx < 0)
        return;                   /* unknown port — silently ignore */

    outb(port + 1, 0x00);         /* Disable all interrupts          */
    outb(port + 3, 0x80);         /* Enable DLAB (set baud divisor)  */
    outb(port + 0, 0x03);         /* Divisor lo: 3 → 38400 baud      */
    outb(port + 1, 0x00);         /* Divisor hi                      */
    outb(port + 3, 0x03);         /* 8 data bits, no parity, 1 stop  */
    outb(port + 2, 0xC7);         /* Enable FIFO, 14-byte threshold  */
    outb(port + 4, 0x0B);         /* IRQs enabled, RTS/DSR set       */

    port_ready[idx] = 1;
}

int serial_is_ready(int port)
{
    int idx = port_idx(port);
    return (idx >= 0) ? port_ready[idx] : 0;
}

void serial_write_char(int port, char c)
{
    if (!serial_is_ready(port))
        return;
    while (!tx_empty(port));
    outb(port, (uint8_t)c);
}

void serial_write_str(int port, const char *s)
{
    if (!s)
        return;
    while (*s)
        serial_write_char(port, *s++);
}

void serial_write_dec(int port, uint32_t n)
{
    char buf[11];   /* 10 digits + NUL */
    int i = 10;
    buf[i] = '\0';
    if (n == 0) {
        buf[--i] = '0';
    } else {
        while (n > 0) {
            buf[--i] = '0' + (char)(n % 10);
            n /= 10;
        }
    }
    serial_write_str(port, &buf[i]);
}

void serial_write_hex(int port, uint32_t n)
{
    static const char hex[] = "0123456789ABCDEF";
    char buf[11];   /* "0x" + 8 hex digits + NUL */
    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 7; i >= 0; i--) {
        buf[2 + i] = hex[n & 0xF];
        n >>= 4;
    }
    buf[10] = '\0';
    serial_write_str(port, buf);
}

char serial_read_char(int port)
{
    while (!rx_ready(port));
    return (char)inb(port);
}

int serial_received(int port)
{
    return rx_ready(port);
}

/* ── Legacy API — thin wrappers targeting SERIAL_DEBUG ──────────────────── */

void init_serial(int ComPort)       { serial_init(ComPort); }
void Serial_WriteChar(char a)       { serial_write_char(SERIAL_DEBUG, a); }
void Serial_WriteString(string a)   { serial_write_str(SERIAL_DEBUG, a); }
void Serial_WriteDec(uint32_t n)    { serial_write_dec(SERIAL_DEBUG, n); }
void Serial_WriteHex(uint32_t n)    { serial_write_hex(SERIAL_DEBUG, n); }
char Serial_ReadChar(void)          { return serial_read_char(SERIAL_DEBUG); }

