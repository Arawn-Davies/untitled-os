# shell — Minimal kernel command shell

**Header:** `kernel/include/kernel/shell.h`  
**Source:** `kernel/arch/i386/shell.c`

Provides an interactive read-eval-print loop (REPL) over the VGA text
terminal and PS/2 keyboard.  It is the first user-facing interface in the
kernel, launched by `kernel_main` after all subsystems have initialised.

---

## How it works

`shell_run` loops forever, printing the prompt `untitled> `, reading a line
of input, splitting it into tokens, and dispatching to a built-in command
handler.

### Input (`shell_readline`)

The internal `shell_readline` function calls `keyboard_getchar` for each
character and echoes it to the terminal via `t_putchar`.  It handles:

- **Backspace (`\b`)** — calls `t_backspace()` and shrinks the buffer.
- **Enter (`\n` / `\r`)** — terminates the line.
- **Printable ASCII (0x20–0x7E)** — appended to the input buffer (up to
  255 characters; additional characters are silently dropped).
- **Everything else** — discarded.

### Parsing (`shell_parse`)

`shell_parse` splits the input line in-place on space characters.  It
produces an `argv`-style array of up to eight `char *` tokens and returns
`argc`.  Empty lines (all spaces) yield `argc == 0` and are skipped.

---

## Built-in commands

| Command | Action |
|---|---|
| `help` | List all built-in commands. |
| `clear` | Re-initialise the VGA terminal and VESA TTY, clearing the screen. |
| `echo [args..]` | Print the arguments separated by spaces, followed by a newline. |
| `meminfo` | Print heap used and heap free (in bytes) via `heap_used()` / `heap_free()`. |
| `uptime` | Print the number of PIT ticks since boot via `timer_get_ticks()`. |
| `shutdown` | Print a shutdown message, disable interrupts (`cli`), and halt the CPU. |

Unknown commands print `Unknown command: <name>` and continue the loop.

---

## Functions

### `shell_run`

```c
void shell_run(void);
```

Enter the interactive shell loop.  Never returns.  Prints a welcome message
(`Makar kernel shell. Type 'help' for a list of commands.`) before the first
prompt.

Must be called after `keyboard_init()`, `heap_init()`, `init_timer()`, and
`terminal_initialize()` have all completed successfully.

---

## Future work

- Add filesystem commands (`ls`, `dir`, `cd`, `cat`) once an ATA/IDE and
  FAT32 driver are in place.
- Add `ver` / `version` command to print the kernel version string.
- Implement a welcome banner / ASCII logo on the `clear` command.
- Port the shell to the serial port so the same commands are available
  over a headless UART connection (matching Medli's serial console model).
- Add command history (up/down arrow navigation) once extended scan codes
  are handled by the keyboard driver.
