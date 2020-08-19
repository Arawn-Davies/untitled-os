#include <kernel/idt.h>
#include <kernel/asm.h>
#include <string.h>

#include <kernel/asm.h>
#include <kernel/isr.h>
#include <kernel/tty.h>
#include <kernel/types.h>
#include <kernel/system.h>

isr_t interrupt_handlers[256];

void init_isr_handlers()
{
	memset(&interrupt_handlers, 0, sizeof(isr_t)*256);
}

void register_interrupt_handler(uint8_t n, isr_t handler)
{
	interrupt_handlers[n] = handler;
	if (n >= IRQ0)
	{
		t_writestring("IRQ ");
		t_dec(n - IRQ0);
	}
	else
	{
		t_writestring("Interrupt ");
		t_dec(n);
	}
	t_writestring(" Registered...\n");
}


void unregister_interrupt_handler(uint8_t n)
{
	t_writestring("Unregistering a handler...\n");
	interrupt_handlers[n] = 0x0;
}

int is_registered(uint8_t n)
{
	return !(interrupt_handlers[n] == 0);
}

// This gets called from our ASM interrupt handler stub.
void isr_handler(registers_t regs)
{
	if(interrupt_handlers[regs.int_no] != 0)
	{
		isr_t handler = interrupt_handlers[regs.int_no];
		handler(regs);
	}
	else
	{
		t_writestring("Unhandled Interrupt: ");
		t_dec(regs.int_no);
		t_putchar('\n');
		PANIC("Unhandled Interrupt");
	}
}

// This gets called from our ASM interrupt handler stub.
void irq_handler(registers_t regs)
{
	if(interrupt_handlers[regs.int_no] != 0)
	{
		isr_t handler = interrupt_handlers[regs.int_no];
		handler(regs);
	}

	// Send an EOI (end of interrupt) signal to the PICS.
	// If this interrupt involved the slave
	if (regs.int_no >= 40)
	{
		// Send reset signal to slave.
		outb(0xA0, 0x20);
	}

	// Send reset signal to master. (As well as slave, if necessary).
	outb(0x20, 0x20);
}