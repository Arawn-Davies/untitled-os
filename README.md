# Makar

[![Build](https://github.com/Arawn-Davies/Makar/actions/workflows/build.yml/badge.svg)](https://github.com/Arawn-Davies/Makar/actions/workflows/build.yml)

Makar is a bare-metal i686 operating system written in C/C++.  It is the GCC
sibling of [Medli](https://github.com/Arawn-Davies/Medli) — two independent
implementations of the same OS, sharing UX conventions, filesystem design,
and long-term binary format goals.

Built with the i686-elf-gcc cross compiler
([Quick-i686](https://github.com/Arawn-Davies/quick-i686)) and booted via
GRUB Multiboot 2.

### Medli

[Medli](https://github.com/Arawn-Davies/Medli) is the C# / Cosmos
counterpart of Makar.  Both projects implement the same operating system
concept — a shared command vocabulary, filesystem layout, and service
model — in different languages and runtimes.  See the
[Makar × Medli](docs/makar-medli.md) roadmap for details.

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
- ATA/IDE PIO driver (28-bit LBA, polling, read + write, 4 drives)
- MBR partition table — read (`lspart`) + interactive creation (`mkpart mbr`),
  MDFS type `0xFA` supported
- GPT partition table — read + interactive creation (`mkpart gpt`) with
  CRC32-signed headers, auto-generated UUIDs, and known type GUIDs
  (FAT32 / EFI / Linux / MDFS)
- Kernel shell (`help`, `clear`, `echo`, `meminfo`, `uptime`, `shutdown`,
  `lsdisks`, `lspart`, `mkpart`, `readsector`)

## Documentation

Everything you need to build, run, test, and understand Makar lives in
[`docs/`](docs/index.md):

| Guide | |
|---|---|
| [Building](docs/building.md) | Prerequisites, build scripts, Docker, Docker Compose |
| [Testing](docs/testing.md) | Serial smoke test, GDB boot-test suite |
| [WSL2](docs/wsl2.md) | Windows development via WSL2 + Docker Desktop |
| [Makar × Medli](docs/makar-medli.md) | Co-operation roadmap with the Medli project |

Kernel subsystem and libc documentation is in [`docs/index.md`](docs/index.md).
