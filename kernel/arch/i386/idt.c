#include <kernel/idt.h>
#include <kernel/asm.h>
#include <stdio.h>
#include <string.h>
#include <kernel/isr.h>

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

// Function arch/i386/idt.S, loads IDT from a pointer to an idt_ptr
extern void idt_load(struct idt_ptr *idt_ptr_addr);

/* ISRs */
DECLARE_INTERRUPT_HANDLER(0);
DECLARE_INTERRUPT_HANDLER(1);
DECLARE_INTERRUPT_HANDLER(2);
DECLARE_INTERRUPT_HANDLER(3);
DECLARE_INTERRUPT_HANDLER(4);
DECLARE_INTERRUPT_HANDLER(5);
DECLARE_INTERRUPT_HANDLER(6);
DECLARE_INTERRUPT_HANDLER(7);
DECLARE_INTERRUPT_HANDLER(8);
DECLARE_INTERRUPT_HANDLER(9);
DECLARE_INTERRUPT_HANDLER(10);
DECLARE_INTERRUPT_HANDLER(11);
DECLARE_INTERRUPT_HANDLER(12);
DECLARE_INTERRUPT_HANDLER(13);
DECLARE_INTERRUPT_HANDLER(14);
DECLARE_INTERRUPT_HANDLER(15);
DECLARE_INTERRUPT_HANDLER(16);
DECLARE_INTERRUPT_HANDLER(17);
DECLARE_INTERRUPT_HANDLER(18);
DECLARE_INTERRUPT_HANDLER(19);

/* IRQs */
DECLARE_INTERRUPT_HANDLER(32);
DECLARE_INTERRUPT_HANDLER(33);
DECLARE_INTERRUPT_HANDLER(34);
DECLARE_INTERRUPT_HANDLER(35);
DECLARE_INTERRUPT_HANDLER(36);
DECLARE_INTERRUPT_HANDLER(37);
DECLARE_INTERRUPT_HANDLER(38);
DECLARE_INTERRUPT_HANDLER(39);
DECLARE_INTERRUPT_HANDLER(40);
DECLARE_INTERRUPT_HANDLER(41);
DECLARE_INTERRUPT_HANDLER(42);
DECLARE_INTERRUPT_HANDLER(43);
DECLARE_INTERRUPT_HANDLER(44);
DECLARE_INTERRUPT_HANDLER(45);
DECLARE_INTERRUPT_HANDLER(46);
DECLARE_INTERRUPT_HANDLER(47);

void set_idt_entry(uint8_t num, uint64_t handler, uint16_t sel, uint8_t flags)
{
	idt[num].handler_lo = handler & 0xFFFF;
	idt[num].handler_hi = (handler >> 16) & 0xFFFF;
	idt[num].always0 = 0;
	idt[num].flags = flags;
	idt[num].sel = sel;
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

	set_idt_entry(0, (uint32_t)ISR0, 0x08, 0x8E);
	set_idt_entry(1, (uint32_t)ISR1, 0x08, 0x8E);
	set_idt_entry(2, (uint32_t)ISR2, 0x08, 0x8E);
	set_idt_entry(3, (uint32_t)ISR3, 0x08, 0x8E);
	set_idt_entry(4, (uint32_t)ISR4, 0x08, 0x8E);
	set_idt_entry(5, (uint32_t)ISR5, 0x08, 0x8E);
	set_idt_entry(6, (uint32_t)ISR6, 0x08, 0x8E);
	set_idt_entry(7, (uint32_t)ISR7, 0x08, 0x8E);
	set_idt_entry(8, (uint32_t)ISR8, 0x08, 0x8E);
	set_idt_entry(9, (uint32_t)ISR9, 0x08, 0x8E);
	set_idt_entry(10, (uint32_t)ISR10, 0x08, 0x8E);
	set_idt_entry(11, (uint32_t)ISR11, 0x08, 0x8E);
	set_idt_entry(12, (uint32_t)ISR12, 0x08, 0x8E);
	set_idt_entry(13, (uint32_t)ISR13, 0x08, 0x8E);
	set_idt_entry(14, (uint32_t)ISR14, 0x08, 0x8E);
	set_idt_entry(15, (uint32_t)ISR15, 0x08, 0x8E);
	set_idt_entry(16, (uint32_t)ISR16, 0x08, 0x8E);
	set_idt_entry(17, (uint32_t)ISR17, 0x08, 0x8E);
	set_idt_entry(18, (uint32_t)ISR18, 0x08, 0x8E);
	set_idt_entry(19, (uint32_t)ISR19, 0x08, 0x8E);
	set_idt_entry(20, (uint32_t)ISR20, 0x08, 0x8E);
	set_idt_entry(21, (uint32_t)ISR21, 0x08, 0x8E);
	set_idt_entry(22, (uint32_t)ISR22, 0x08, 0x8E);
	set_idt_entry(23, (uint32_t)ISR23, 0x08, 0x8E);
	set_idt_entry(24, (uint32_t)ISR24, 0x08, 0x8E);
	set_idt_entry(25, (uint32_t)ISR25, 0x08, 0x8E);
	set_idt_entry(26, (uint32_t)ISR26, 0x08, 0x8E);
	set_idt_entry(27, (uint32_t)ISR27, 0x08, 0x8E);
	set_idt_entry(28, (uint32_t)ISR28, 0x08, 0x8E);
	set_idt_entry(29, (uint32_t)ISR29, 0x08, 0x8E);
	set_idt_entry(30, (uint32_t)ISR30, 0x08, 0x8E);
	set_idt_entry(31, (uint32_t)ISR31, 0x08, 0x8E);
	set_idt_entry(32, (uint32_t)IRQ0, 0x08, 0x8E);
	set_idt_entry(33, (uint32_t)IRQ1, 0x08, 0x8E);
	set_idt_entry(34, (uint32_t)IRQ2, 0x08, 0x8E);
	set_idt_entry(35, (uint32_t)IRQ3, 0x08, 0x8E);
	set_idt_entry(36, (uint32_t)IRQ4, 0x08, 0x8E);
	set_idt_entry(37, (uint32_t)IRQ5, 0x08, 0x8E);
	set_idt_entry(38, (uint32_t)IRQ6, 0x08, 0x8E);
	set_idt_entry(39, (uint32_t)IRQ7, 0x08, 0x8E);
	set_idt_entry(40, (uint32_t)IRQ8, 0x08, 0x8E);
	set_idt_entry(41, (uint32_t)IRQ9, 0x08, 0x8E);
	set_idt_entry(42, (uint32_t)IRQ10, 0x08, 0x8E);
	set_idt_entry(43, (uint32_t)IRQ11, 0x08, 0x8E);
	set_idt_entry(44, (uint32_t)IRQ12, 0x08, 0x8E);
	set_idt_entry(45, (uint32_t)IRQ13, 0x08, 0x8E);
	set_idt_entry(46, (uint32_t)IRQ14, 0x08, 0x8E);
	set_idt_entry(47, (uint32_t)IRQ15, 0x08, 0x8E);

	// Points the processor's internal register to the new IDT
	idt_load(&idtp);
	printf("Interrupt Descriptor Table installed.\n");
}
