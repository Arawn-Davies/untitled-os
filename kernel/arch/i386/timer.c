#include <kernel/timer.h>
#include <kernel/isr.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <kernel/tty.h>
#include <kernel/asm.h>

uint32_t tick = 0;

void timer_callback(registers_t regs)
{
	tick++;
	t_writestring("Tick: ");
	t_writestring(tick);
	t_writestring("\n");
}

void init_timer(uint32_t frequency)
{
	t_writestring("Timer pre-register\n");
	// Firstly, register our timer callback.
	register_interrupt_handler(IRQ0, &timer_callback);
	t_writestring("Timer post-register\n");

	// The value we send to the PIT is the value to divide it's input clock
	// (1193180 Hz) by, to get our required frequency. Important to note is
	// that the divisor must be small enough to fit into 16-bits.
	uint32_t divisor = 1193180 / frequency;

	// Send the command byte.
	outb(0x43, 0x36);

	// Divisor has to be sent byte-wise, so split here into upper/lower bytes.
	uint8_t l = (uint8_t)(divisor & 0xFF);
	uint8_t h = (uint8_t)((divisor >> 8) & 0xFF);

	// Send the frequency divisor.
	outb(0x40, l);
	outb(0x40, h);
	t_writestring("Timer init done!\n");
}