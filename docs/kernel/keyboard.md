# keyboard — PS/2 keyboard driver

**Header:** `kernel/include/kernel/keyboard.h`  
**Source:** `kernel/arch/i386/keyboard.c`

Handles IRQ 1 from the PS/2 keyboard controller, translates scan-code set 1
to ASCII using a US QWERTY layout, tracks shift and caps-lock state, and
stores incoming characters in a 256-byte ring buffer for consumption by the
kernel shell.

---

## How it works

The PS/2 keyboard controller fires IRQ 1 whenever a key is pressed or
released.  The interrupt handler reads the raw scan code from port `0x60`
(the PS/2 data port) and:

1. **Modifier tracking** — detects left/right shift press and release (`0x2A`,
   `0x36`, and their release variants) and caps-lock toggle (`0x3A`).  State
   is kept in the module-static variables `shift_pressed` and `caps_lock_on`.
2. **Release filtering** — any scan code with bit 7 set (other than shift
   releases) is discarded; only key-press events are processed.
3. **Translation** — looks up the scan code in one of two 89-entry ASCII
   tables: `sc_ascii_lower` (unshifted) and `sc_ascii_upper` (shifted).
   Caps-lock XOR shift governs letter case; only shift governs symbol keys.
4. **Ring buffer** — the resulting character is pushed into a 256-byte
   power-of-two ring buffer.  If the buffer is full the character is silently
   dropped.

---

## Functions

### `keyboard_init`

```c
void keyboard_init(void);
```

Register the IRQ 1 handler via `register_interrupt_handler(IRQ1, ...)`.
Must be called after `init_descriptor_tables()` has set up the IDT.

### `keyboard_getchar`

```c
char keyboard_getchar(void);
```

Block until at least one character is available in the ring buffer, then
return and remove it.  Uses a `pause`-hinted spin loop so the CPU does not
busy-spin at full power while waiting.

Used by `shell_readline` to read user input one character at a time.

### `keyboard_poll`

```c
char keyboard_poll(void);
```

Non-blocking read.  Returns the next character from the ring buffer, or `0`
if the buffer is empty.  Suitable for polling loops that must continue other
work while waiting for input.

---

## Ring buffer

The buffer is a 256-byte array indexed by two `uint8_t` counters (`head` for
writes, `tail` for reads).  Because both counters are `uint8_t` and the
buffer size is a power of two, natural wrapping eliminates the need for an
explicit modulo operation.  The buffer is considered full when
`head - tail == 255` (one slot is always kept empty to distinguish full from
empty).

---

## Future work

- Extend the scan-code tables to cover extended keys (E0-prefixed: arrow
  keys, Home, End, Page Up/Down, Insert, Delete).
- Add Ctrl modifier tracking for control-character sequences (`Ctrl-C`,
  `Ctrl-D`, etc.) needed by a future interactive shell.
- Provide an interrupt-driven blocking wait (condition variable or semaphore)
  rather than a spin loop once a scheduler exists.
