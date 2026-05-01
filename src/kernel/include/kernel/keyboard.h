#ifndef _KERNEL_KEYBOARD_H
#define _KERNEL_KEYBOARD_H

#include <stdint.h>

/*
 * Sentinels for extended (non-ASCII) keys — high-byte range so they never
 * collide with any Ctrl+letter code (0x01-0x1A).
 */
#define KEY_ARROW_UP    ((char)0x80)
#define KEY_ARROW_DOWN  ((char)0x81)
#define KEY_ARROW_LEFT  ((char)0x82)
#define KEY_ARROW_RIGHT ((char)0x83)

/* Ctrl+C sentinel returned by keyboard_getchar() when a sigint fires. */
#define KEY_CTRL_C      ((char)0x03)

/* Pane IDs for keyboard_bind_pane() / keyboard_focus_pane(). */
#define KB_PANE_TOP     0
#define KB_PANE_BOTTOM  1

void keyboard_init(void);
char keyboard_getchar(void);
char keyboard_poll(void);

/* Per-task input routing (Phase 2 / split-panes). */
void keyboard_bind_pane(int pane_id, struct task *t);
void keyboard_focus_pane(int pane_id);
void keyboard_set_focus(struct task *t);

/*
 * keyboard_sigint_consume – atomically read and clear the Ctrl+C flag.
 * Returns 1 if Ctrl+C was pressed since the last call, 0 otherwise.
 * Used by cmd_exec to force-kill a running user task.
 */
int keyboard_sigint_consume(void);

#endif /* _KERNEL_KEYBOARD_H */
