#ifndef _KERNEL_ASM_H
#define _KERNEL_ASM_H

#include <stdint.h>

inline void outb(uint16_t port, uint8_t val)
{
	asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

inline uint8_t inb(uint16_t port)
{
	uint8_t ret;
	asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
	return ret;
}

/* 16-bit I/O — used by ATA/IDE data register (0x1F0 / 0x170). */
inline void outw(uint16_t port, uint16_t val)
{
	asm volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}

inline uint16_t inw(uint16_t port)
{
	uint16_t ret;
	asm volatile("inw %1, %0" : "=a"(ret) : "Nd"(port));
	return ret;
}

/*
 * io_wait — insert a ~1 µs delay by writing to the POST diagnostic port.
 * Port 0x80 is unused after POST and safe to write on all PC hardware.
 * Required after certain I/O sequences (e.g. PIC remapping, ATA commands).
 */
inline void io_wait(void)
{
	outb(0x80, 0);
}

inline void enable_interrupts(void) { asm volatile("sti"); }

inline void disable_interrupts(void) { asm volatile("cli"); }

inline void invlpg(void *m)
{
	asm volatile("invlpg (%0)" : : "b"(m) : "memory");
}

#endif