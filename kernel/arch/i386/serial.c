#include <kernel/types.h>
//#include <kernel/system.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <kernel/asm.h>
#include <kernel/serial.h>

#define COM1 0x3f8
#define COM2 0x2f8
#define COM3 0x3e8
#define COM4 0x2e8

int PORT;

enum ComPorts
{
	COMPort1 = 0x3f8,
	COMPort2 = 0x2f8,
	COMPort3 = 0x3e8,
	COMPort4 = 0x2e8
};

int serialReceived() {
   return inb(COM1 + 5) & 1;
}
 
char Serial_ReadChar() {
   while (serialReceived() == 0);
   return inb(COM1);
}

int isTransmitEmpty() {
   return inb(COM1 + 5) & 0x20;
}

//Writes a character over a serial connection
void Serial_WriteChar(char a) {
   while (isTransmitEmpty() == 0);
   outb(COM1,a);
}

//Prints a string over a serial connection
void Serial_WriteString(string a)
{
    while (*a != 0)
    {
        Serial_WriteChar(*a);
        a = a + 1;
    }
}

//Initialises the serial connection
void init_serial(int ComPort) 
{
	printf("Serial init start\n");
	PORT = ComPort;
 	outb(PORT, 0x00);    // Disable all interrupts
 	outb(PORT, 0x80);    // Enable DLAB (set baud rate divisor)
 	outb(PORT, 0x03);    // Set divisor to 3 (lo byte) 38400 baud
 	outb(PORT, 0x00);    //                  (hi byte)
 	outb(PORT, 0x03);    // 8 bits, no parity, one stop bit
 	outb(PORT, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
	outb(PORT, 0x0B);    // IRQs enabled, RTS/DSR set
	printf("Serial init done!\n");
	Serial_WriteString("Serial init done!\n");
}
