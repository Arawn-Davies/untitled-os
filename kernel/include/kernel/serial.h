#ifndef _KERNEL_SERIAL_H_
#define _KERNEL_SERIAL_H_

#include <kernel/types.h>
#include <kernel/asm.h>

#define COM1 0x3f8
#define COM2 0x2f8
#define COM3 0x3e8
#define COM4 0x2e8

void init_serial(int ComPort);
char Serial_ReadChar();
void Serial_WriteChar(char a);
void Serial_WriteString(string a);

#endif
