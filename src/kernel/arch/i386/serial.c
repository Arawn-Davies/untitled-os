#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <kernel/asm.h>
#include <kernel/serial.h>

/* Active port — set by init_serial(); all helpers use this. */
static int PORT = COM1;

int serialReceived(void) {
    return inb(PORT + 5) & 1;
}

char Serial_ReadChar(void) {
    while (serialReceived() == 0);
    return inb(PORT);
}

int isTransmitEmpty(void) {
    return inb(PORT + 5) & 0x20;
}

/* Write a single character over the active serial port. */
void Serial_WriteChar(char a) {
    while (isTransmitEmpty() == 0);
    outb(PORT, a);
}

/* Write a NUL-terminated string over the active serial port. */
void Serial_WriteString(string a)
{
    while (*a != '\0') {
        Serial_WriteChar(*a);
        a++;
    }
}

/* Write an unsigned 32-bit integer in decimal over the active serial port. */
void Serial_WriteDec(uint32_t n)
{
    char buf[11]; /* max uint32 is 4294967295 (10 digits) + NUL */
    int i = 10;
    buf[i] = '\0';
    if (n == 0) {
        buf[--i] = '0';
    } else {
        while (n > 0) {
            buf[--i] = '0' + (n % 10);
            n /= 10;
        }
    }
    Serial_WriteString(&buf[i]);
}

/* Write an unsigned 32-bit integer in hexadecimal (0xXXXXXXXX) over serial. */
void Serial_WriteHex(uint32_t n)
{
    static const char hexdigits[] = "0123456789ABCDEF";
    char buf[11]; /* "0x" + 8 hex digits + NUL */
    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 7; i >= 0; i--) {
        buf[2 + i] = hexdigits[n & 0xF];
        n >>= 4;
    }
    buf[10] = '\0';
    Serial_WriteString(buf);
}

/* Initialise the given COM port at 38400 8N1 with FIFOs enabled. */
void init_serial(int ComPort)
{
    PORT = ComPort;
    outb(PORT + 1, 0x00); /* Disable all interrupts        */
    outb(PORT + 3, 0x80); /* Enable DLAB (set baud divisor) */
    outb(PORT + 0, 0x03); /* Divisor lo: 3 → 38400 baud    */
    outb(PORT + 1, 0x00); /* Divisor hi                    */
    outb(PORT + 3, 0x03); /* 8 bits, no parity, 1 stop bit */
    outb(PORT + 2, 0xC7); /* Enable FIFO, 14-byte threshold */
    outb(PORT + 4, 0x0B); /* IRQs enabled, RTS/DSR set     */
    Serial_WriteString("serial: COM1 ready\n");
}
