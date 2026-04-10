# Makar

Makar is a bare-metal i686 operating system written in C/C++.  It is the GCC
sibling of [Medli](https://github.com/Arawn-Davies/Medli) — two independent
implementations of the same OS, sharing UX conventions, filesystem design,
and long-term binary format goals.

Built with the i686-elf-gcc cross compiler
([Quick-i686](https://github.com/Arawn-Davies/quick-i686)) and booted via
GRUB Multiboot 2.

## What's implemented

- Serial (UART, 38400 baud, COM1)
- GDT + IDT
- ISR / IRQ dispatch
- GRUB Multiboot 2 boot
- VGA text terminal
- VESA linear framebuffer + bitmap-font text renderer
- Physical memory manager (bitmap allocator)
- Paging (identity-mapped 0–8 MiB, extensible)
- Kernel heap (`kmalloc` / `kfree` / `krealloc`)
- PIT timer (50 Hz) + `ksleep`
- INT 1 / INT 3 debug handlers (GDB-friendly)
- PS/2 keyboard driver (IRQ 1, scan-code set 1, ring buffer)
- Kernel shell (`help`, `clear`, `echo`, `meminfo`, `uptime`, `shutdown`)

## Documentation

See [`docs/`](docs/index.md) for full subsystem documentation and the
[Makar × Medli](docs/makar-medli.md) co-operation roadmap.

## Build

```sh
bash iso.sh        # build makar.iso
bash qemu.sh       # build and run in QEMU
bash gdb.sh        # build and launch with GDB stub on :1234
```

Requires: `i686-elf-gcc`, `nasm`, `grub-mkrescue`, `xorriso`, `qemu-system-i386`
