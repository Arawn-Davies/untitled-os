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
void Serial_WriteDec(uint32_t n);
void Serial_WriteHex(uint32_t n);

/*
 * Lightweight serial-logging macros for development/debug builds.
 *
 * Include this header and compile with -DDEV_BUILD to enable verbose serial
 * output from every kernel subsystem.  In release builds these expand to
 * no-ops so there is no runtime overhead.
 */
#ifdef DEV_BUILD
#  define KLOG(msg)    Serial_WriteString(msg)
#  define KLOG_DEC(n)  Serial_WriteDec(n)
#  define KLOG_HEX(n)  Serial_WriteHex(n)
#else
#  define KLOG(msg)    ((void)0)
#  define KLOG_DEC(n)  ((void)0)
#  define KLOG_HEX(n)  ((void)0)
#endif

#endif
