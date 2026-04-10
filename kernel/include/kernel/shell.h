#ifndef _KERNEL_SHELL_H
#define _KERNEL_SHELL_H

/*
 * Minimal kernel REPL over VGA + PS/2 keyboard.
 *
 * shell_run() must be called after all subsystems (keyboard, heap, timer,
 * VGA) have been initialised.  It never returns.
 */
void shell_run(void);

#endif /* _KERNEL_SHELL_H */
