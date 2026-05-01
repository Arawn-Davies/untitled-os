DEFAULT_HOST!=../default-host.sh
HOST?=$(DEFAULT_HOST)

CC=$(HOST)-gcc
AS=$(HOST)-as
LD=$(HOST)-ld

CFLAGS?=-O2 -g
CFLAGS:=$(CFLAGS) -std=c11 -ffreestanding -fno-stack-protector \
        -Wall -Wextra -nostdlib -nostdinc -I.

LDFLAGS=-T link.ld -nostdlib

DESTDIR?=
ISODIR?=$(CURDIR)/../../isodir

PROGS=hello.elf calc.elf

all: $(PROGS)

hello.elf: crt0.o hello.o
	$(LD) $(LDFLAGS) -o $@ $^

calc.elf: crt0.o calc.o
	$(LD) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.S
	$(AS) --32 -o $@ $<

install: all
	mkdir -p $(ISODIR)/apps
	cp $(PROGS) $(ISODIR)/apps/

install-headers:

clean:
	rm -f *.o $(PROGS)

.PHONY: all install install-headers clean
