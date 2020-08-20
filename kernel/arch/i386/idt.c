#include <kernel/idt.h>
#include <kernel/asm.h>
#include <stdio.h>
#include <string.h>
#include <kernel/interrupts.h>

#define SET_IDT_ENTRY(idx)                                 \
	set_idt_entry(idx, (uint32_t)&interrupt_handler_##idx, \
				  0x08, 0x8E);

#define DECLARE_INTERRUPT_HANDLER(i) void interrupt_handler_##i(void)

// Defines an IDT entry
struct idt_entry
{
	uint16_t handler_lo;
	uint16_t sel;
	uint8_t always0;
	uint8_t flags;
	uint16_t handler_hi;
} __attribute__((packed));
typedef struct idt_entry idt_entry_t;

struct idt_ptr
{
	uint16_t limit;
	uint32_t base;
} __attribute__((packed));
typedef struct idt_ptr idt_ptr_t;

// Declare an IDT of 256 entries.
idt_entry_t idt[IDT_NUM_ENTRIES];
idt_ptr_t idtp;

// Function arch/i386/dt_asm.S, loads IDT from a pointer to an idt_ptr
extern void idt_load(struct idt_ptr *idt_ptr_addr);

void set_idt_entry(uint8_t num, uint64_t handler, uint16_t sel, uint8_t flags)
{
	idt[num].handler_lo = handler & 0xFFFF;
	idt[num].handler_hi = (handler >> 16) & 0xFFFF;

	idt[num].sel		= sel;
	idt[num].always0	= 0;
	// We must uncomment the OR below when we get to using user-mode.
	// It sets the interrupt gate's privilege level to 3.
	idt[num].flags		= flags /* | 0x60 */;
}

// Installs the IDT
void idt_install()
{
	// Sets the special IDT pointer up
	idtp.limit = (sizeof(struct idt_entry) * IDT_NUM_ENTRIES) - 1;
	idtp.base = (uint32_t)&idt;

	// Clear out the entire IDT, initializing it to zeros
	memset(&idt, 0, sizeof(struct idt_entry) * IDT_NUM_ENTRIES);

	//remap IRQ table
	outb(0x20, 0x11);
	outb(0xA0, 0x11);
	outb(0x21, 0x20);
	outb(0xA1, 0x28);
	outb(0x21, 0x04);
	outb(0xA1, 0x02);
	outb(0x21, 0x01);
	outb(0xA1, 0x01);
	outb(0x21, 0x0);
	outb(0xA1, 0x0);

	set_idt_entry(0, (uint32_t)isr0, 0x08, 0x8E);
	set_idt_entry(1, (uint32_t)isr1, 0x08, 0x8E);
	set_idt_entry(2, (uint32_t)isr2, 0x08, 0x8E);
	set_idt_entry(3, (uint32_t)isr3, 0x08, 0x8E);
	set_idt_entry(4, (uint32_t)isr4, 0x08, 0x8E);
	set_idt_entry(5, (uint32_t)isr5, 0x08, 0x8E);
	set_idt_entry(6, (uint32_t)isr6, 0x08, 0x8E);
	set_idt_entry(7, (uint32_t)isr7, 0x08, 0x8E);
	set_idt_entry(8, (uint32_t)isr8, 0x08, 0x8E);
	set_idt_entry(9, (uint32_t)isr9, 0x08, 0x8E);
	set_idt_entry(10, (uint32_t)isr10, 0x08, 0x8E);
	set_idt_entry(11, (uint32_t)isr11, 0x08, 0x8E);
	set_idt_entry(12, (uint32_t)isr12, 0x08, 0x8E);
	set_idt_entry(13, (uint32_t)isr13, 0x08, 0x8E);
	set_idt_entry(14, (uint32_t)isr14, 0x08, 0x8E);
	set_idt_entry(15, (uint32_t)isr15, 0x08, 0x8E);
	set_idt_entry(16, (uint32_t)isr16, 0x08, 0x8E);
	set_idt_entry(17, (uint32_t)isr17, 0x08, 0x8E);
	set_idt_entry(18, (uint32_t)isr18, 0x08, 0x8E);
	set_idt_entry(19, (uint32_t)isr19, 0x08, 0x8E);
	set_idt_entry(20, (uint32_t)isr20, 0x08, 0x8E);
	set_idt_entry(21, (uint32_t)isr21, 0x08, 0x8E);
	set_idt_entry(22, (uint32_t)isr22, 0x08, 0x8E);
	set_idt_entry(23, (uint32_t)isr23, 0x08, 0x8E);
	set_idt_entry(24, (uint32_t)isr24, 0x08, 0x8E);
	set_idt_entry(25, (uint32_t)isr25, 0x08, 0x8E);
	set_idt_entry(26, (uint32_t)isr26, 0x08, 0x8E);
	set_idt_entry(27, (uint32_t)isr27, 0x08, 0x8E);
	set_idt_entry(28, (uint32_t)isr28, 0x08, 0x8E);
	set_idt_entry(29, (uint32_t)isr29, 0x08, 0x8E);
	set_idt_entry(30, (uint32_t)isr30, 0x08, 0x8E);
	set_idt_entry(31, (uint32_t)isr31, 0x08, 0x8E);

	set_idt_entry(32, (uint32_t)irq0, 0x08, 0x8E);
	set_idt_entry(33, (uint32_t)irq1, 0x08, 0x8E);
	set_idt_entry(34, (uint32_t)irq2, 0x08, 0x8E);
	set_idt_entry(35, (uint32_t)irq3, 0x08, 0x8E);
	set_idt_entry(36, (uint32_t)irq4, 0x08, 0x8E);
	set_idt_entry(37, (uint32_t)irq5, 0x08, 0x8E);
	set_idt_entry(38, (uint32_t)irq6, 0x08, 0x8E);
	set_idt_entry(39, (uint32_t)irq7, 0x08, 0x8E);
	set_idt_entry(40, (uint32_t)irq8, 0x08, 0x8E);
	set_idt_entry(41, (uint32_t)irq9, 0x08, 0x8E);
	set_idt_entry(42, (uint32_t)irq10, 0x08, 0x8E);
	set_idt_entry(43, (uint32_t)irq11, 0x08, 0x8E);
	set_idt_entry(44, (uint32_t)irq12, 0x08, 0x8E);
	set_idt_entry(45, (uint32_t)irq13, 0x08, 0x8E);
	set_idt_entry(46, (uint32_t)irq14, 0x08, 0x8E);
	set_idt_entry(47, (uint32_t)irq15, 0x08, 0x8E);

	// Points the processor's internal register to the new IDT
	idt_load(&idtp);
	printf("Interrupt Descriptor Table installed.\n");
}
