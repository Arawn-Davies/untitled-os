# userspace/Makefile
#
# Builds all Makar Ring-3 utility programs and installs them into
# $(DESTDIR)/boot/apps/ so that iso.sh can copy them into the ISO.
#
# Each utility is a freestanding i686 ELF32 binary linked against a minimal
# crt0 that calls main() and exits via int 0x80 (SYS_EXIT).
#
# Prerequisites: i686-elf-gcc cross-compiler (same toolchain as the kernel).

DEFAULT_HOST!=../default-host.sh
HOST?=DEFAULT_HOST
HOSTARCH!=../target-triplet-to-arch.sh $(HOST)

CFLAGS?=-O2 -g
CPPFLAGS?=
LDFLAGS?=
LIBS?=

DESTDIR?=
PREFIX?=/usr/local
BOOTDIR?=/boot
APPSDIR?=$(BOOTDIR)/apps

AR=$(HOST)-ar
AS=$(HOST)-as
CC=$(HOST)-gcc

ARCHDIR=arch/$(HOSTARCH)

# Freestanding build: no hosted headers, no standard libraries.
CFLAGS:=$(CFLAGS) -ffreestanding -Wall -Wextra -std=gnu11
CPPFLAGS:=$(CPPFLAGS) -Iinclude
LDFLAGS:=$(LDFLAGS) -nostdlib -T $(ARCHDIR)/linker.ld

CRT0_OBJ=$(ARCHDIR)/crt0.o

# -----------------------------------------------------------------------
# List of utility programs to build.  Add new entries here and in the
# per-binary rules below when adding more utilities.
# -----------------------------------------------------------------------
BINS=\
echo \
true \
false \

.PHONY: all clean install install-headers
.SUFFIXES: .o .c .S

all: $(BINS)

# -----------------------------------------------------------------------
# crt0 — shared ELF entry stub
# -----------------------------------------------------------------------
$(CRT0_OBJ): $(ARCHDIR)/crt0.S
	$(CC) -MD -c $< -o $@ $(CFLAGS) $(CPPFLAGS)

# -----------------------------------------------------------------------
# Per-binary compilation and link rules
# -----------------------------------------------------------------------
echo/echo.o: echo/echo.c
	$(CC) -MD -c $< -o $@ $(CFLAGS) $(CPPFLAGS)

echo: echo/echo.o $(CRT0_OBJ)
	$(CC) -o $@ $(CFLAGS) $(LDFLAGS) $(CRT0_OBJ) $<

true/true.o: true/true.c
	$(CC) -MD -c $< -o $@ $(CFLAGS) $(CPPFLAGS)

true: true/true.o $(CRT0_OBJ)
	$(CC) -o $@ $(CFLAGS) $(LDFLAGS) $(CRT0_OBJ) $<

false/false.o: false/false.c
	$(CC) -MD -c $< -o $@ $(CFLAGS) $(CPPFLAGS)

false: false/false.o $(CRT0_OBJ)
	$(CC) -o $@ $(CFLAGS) $(LDFLAGS) $(CRT0_OBJ) $<

# -----------------------------------------------------------------------
# Install
# -----------------------------------------------------------------------
install: all
	mkdir -p $(DESTDIR)$(APPSDIR)
	cp $(BINS) $(DESTDIR)$(APPSDIR)/

install-headers:

# -----------------------------------------------------------------------
# Clean
# -----------------------------------------------------------------------
clean:
	rm -f $(BINS)
	rm -f $(CRT0_OBJ) echo/echo.o true/true.o false/false.o
	rm -f $(ARCHDIR)/*.d echo/*.d true/*.d false/*.d

-include $(ARCHDIR)/*.d echo/*.d true/*.d false/*.d
