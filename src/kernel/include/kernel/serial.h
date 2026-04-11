#ifndef _KERNEL_SERIAL_H_
#define _KERNEL_SERIAL_H_

/*
 * serial.h — multi-port serial driver.
 *
 * Each of the four standard PC COM ports can be independently initialised
 * and used concurrently.  Use the role aliases below to keep call-sites
 * readable; change the aliases here if you ever need to remap a role to a
 * different physical port.
 *
 *   SERIAL_DEBUG  –  kernel debug / KLOG output        (COM1, 0x3F8)
 *   SERIAL_SHELL  –  secondary interactive serial REPL  (COM2, 0x2F8)
 *   SERIAL_TERM   –  terminal-emulation / ANSI console  (COM3, 0x3E8)
 *
 * All ports run at 38400 8N1 with FIFOs enabled.
 */

#include <kernel/types.h>
#include <kernel/asm.h>

/* ── COM port I/O base addresses ─────────────────────────────────────────── */

#define COM1  0x3F8
#define COM2  0x2F8
#define COM3  0x3E8
#define COM4  0x2E8

/* ── Role assignments ────────────────────────────────────────────────────── */

#define SERIAL_DEBUG  COM1   /* kernel debug / KLOG output              */
#define SERIAL_SHELL  COM2   /* serial terminal shell (future REPL)     */
#define SERIAL_TERM   COM3   /* terminal emulation / external console   */

/* ── Per-port API ─────────────────────────────────────────────────────────── */

/*
 * serial_init — initialise one COM port at 38400 8N1 with FIFOs enabled.
 * Safe to call multiple times for different ports; each call is independent.
 * `port` must be one of COM1–COM4.
 */
void serial_init(int port);

/* Return 1 if `port` has been initialised by serial_init(), 0 otherwise. */
int  serial_is_ready(int port);

/* Blocking write of one character to `port`. No-op if port not initialised. */
void serial_write_char(int port, char c);

/* Write a NUL-terminated string to `port`. */
void serial_write_str(int port, const char *s);

/* Write an unsigned 32-bit integer in decimal to `port`. */
void serial_write_dec(int port, uint32_t n);

/* Write an unsigned 32-bit integer as "0xXXXXXXXX" to `port`. */
void serial_write_hex(int port, uint32_t n);

/* Blocking read of one character from `port`. */
char serial_read_char(int port);

/* Return 1 if a byte is waiting in `port`'s receive buffer, 0 otherwise. */
int  serial_received(int port);

/* ── Legacy API — thin wrappers targeting SERIAL_DEBUG ──────────────────── */
/*
 * These exist so older call-sites continue to compile unchanged.
 * New code should use the per-port functions above.
 */
void init_serial(int ComPort);     /* calls serial_init(ComPort)          */
char Serial_ReadChar(void);        /* serial_read_char(SERIAL_DEBUG)      */
void Serial_WriteChar(char a);     /* serial_write_char(SERIAL_DEBUG, a)  */
void Serial_WriteString(string a); /* serial_write_str(SERIAL_DEBUG, a)   */
void Serial_WriteDec(uint32_t n);  /* serial_write_dec(SERIAL_DEBUG, n)   */
void Serial_WriteHex(uint32_t n);  /* serial_write_hex(SERIAL_DEBUG, n)   */

/* ── KLOG macros ─────────────────────────────────────────────────────────── */
/*
 * Compile with -DDEV_BUILD to emit verbose debug output over SERIAL_DEBUG.
 * In release builds every KLOG* expands to a no-op with zero runtime cost.
 */
#ifdef DEV_BUILD
#  define KLOG(msg)    serial_write_str(SERIAL_DEBUG, (msg))
#  define KLOG_DEC(n)  serial_write_dec(SERIAL_DEBUG, (n))
#  define KLOG_HEX(n)  serial_write_hex(SERIAL_DEBUG, (n))
#else
#  define KLOG(msg)    ((void)0)
#  define KLOG_DEC(n)  ((void)0)
#  define KLOG_HEX(n)  ((void)0)
#endif

#endif /* _KERNEL_SERIAL_H_ */
