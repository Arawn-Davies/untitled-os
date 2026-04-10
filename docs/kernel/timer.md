# timer — PIT timer driver and `ksleep`

**Header:** `kernel/include/kernel/timer.h`  
**Source:** `kernel/arch/i386/timer.c`

Programs the Intel 8253/8254 Programmable Interval Timer (PIT) channel 0 to
fire IRQ 0 at a configurable frequency, and provides a busy-wait sleep
function built on the resulting tick counter.

---

## How it works

The PIT's input clock runs at **1 193 180 Hz**.  To achieve a desired
frequency `f`, a 16-bit divisor `1193180 / f` is written to PIT channel 0
(I/O port `0x40`).  The PIT then fires IRQ 0 every `divisor` clock cycles.

At boot Makar calls `init_timer(50)`, giving a tick rate of **50 Hz** (one
tick every 20 ms).

Each IRQ 0 fires `timer_callback`, which increments the global `tick` counter
and calls `t_spinner_tick(tick)` to animate the terminal spinner.

---

## Functions

### `init_timer`

```c
void init_timer(uint32_t frequency);
```

Configure PIT channel 0 to interrupt at `frequency` Hz and enable CPU
interrupts.

1. Registers `timer_callback` on `IRQ0`.
2. Computes the divisor and writes it to port `0x43` (command) then `0x40`
   (lo byte, hi byte).
3. Calls `enable_interrupts()` — this is the point at which hardware interrupts
   first become active in the boot sequence.

| Parameter | Description |
|---|---|
| `frequency` | Desired interrupt rate in Hz. Must divide evenly into 1 193 180 or the rate will be approximate. |

### `timer_get_ticks`

```c
uint32_t timer_get_ticks(void);
```

Return the current tick count.  The counter starts at 0 and increments by 1
on every timer interrupt.  At 50 Hz it wraps after approximately 993 days of
continuous uptime.

### `ksleep`

```c
void ksleep(uint32_t ticks);
```

Busy-wait until at least `ticks` timer ticks have elapsed since the call.
At 50 Hz, `ksleep(50)` sleeps for approximately one second.

This is a spin-wait — the CPU executes a tight loop and does not yield.  It
is suitable for short post-boot delays but should be replaced with an
interrupt-driven sleep once a scheduler exists.

---

## Future work

- Replace the spin-wait in `ksleep` with an interrupt-driven sleep queue once
  a process scheduler is in place.
- Expose a monotonic clock interface (nanoseconds / seconds since boot) for
  use by higher-level subsystems and a future system-call layer.
- Add a one-shot alarm / callback registration API for kernel timers.
