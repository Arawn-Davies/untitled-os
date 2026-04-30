#ifndef _KERNEL_KEYBOARD_H
#define _KERNEL_KEYBOARD_H

#include <stdint.h>

/*
 * Sentinels for extended (non-ASCII) keys.  Values are in the high-byte
 * range (0x80-0x83) so they do not overlap any Ctrl+letter code (0x01-0x1A).
 */
#define KEY_ARROW_UP    ((char)0x80)  /* PS/2 extended scan code E0 48 */
#define KEY_ARROW_DOWN  ((char)0x81)  /* PS/2 extended scan code E0 50 */
#define KEY_ARROW_LEFT  ((char)0x82)  /* PS/2 extended scan code E0 4B */
#define KEY_ARROW_RIGHT ((char)0x83)  /* PS/2 extended scan code E0 4D */

/*
 * Ctrl+C is handled out-of-band via keyboard_sigint_consume() rather than
 * being pushed into the ring buffer.  keyboard_getchar() returns KEY_CTRL_C
 * when a pending sigint is detected so callers can react without polling.
 *
 * Ctrl+A (0x01) through Ctrl+D (0x04) are now available — they no longer
 * conflict with the arrow-key sentinels.  Ctrl+S (0x13) and Ctrl+Q (0x11)
 * are used by VICS; Ctrl+A (0x01) is reserved for the split-pane prefix.
 */
#define KEY_CTRL_C      ((char)0x03)  /* out-of-band SIGINT sentinel       */

/* Initialise the PS/2 keyboard driver and register the IRQ1 handler. */
void keyboard_init(void);

/* Block until a character (or Ctrl+C) is available, then return it. */
char keyboard_getchar(void);

/* Return the next character from the ring buffer, or 0 if empty. */
char keyboard_poll(void);

/*
 * keyboard_sigint_consume – atomically read and clear the Ctrl+C flag.
 *
 * Returns 1 if Ctrl+C was pressed since the last call, 0 otherwise.
 * Used by cmd_exec to hard-kill a user task that is not blocked in sys_read.
 */
int keyboard_sigint_consume(void);

#endif /* _KERNEL_KEYBOARD_H */
