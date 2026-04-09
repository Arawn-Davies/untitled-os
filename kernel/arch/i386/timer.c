#include <kernel/timer.h>
#include <kernel/isr.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <kernel/asm.h>

static volatile uint32_t tick = 0;

void timer_callback(registers_t regs)
{
	(void)regs;
	tick++;
}

uint32_t timer_get_ticks(void)
{
	return tick;
}

void ksleep(uint32_t ticks)
{
	/* Note: wraps safely since both values are uint32_t and
	 * the comparison handles the common case where ticks is small. */
	uint32_t end = tick + ticks;
	while (tick < end)
		;
}

void init_timer(uint32_t frequency)
{
	// Firstly, register our timer callback.
	register_interrupt_handler(IRQ0, &timer_callback);

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

	// Multiboot 2 enters the kernel with interrupts disabled.  Enable them
	// now that the IDT and PIT are fully configured so that IRQs can fire.
	enable_interrupts();
}