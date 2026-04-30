# Makar

[![CI](https://github.com/Arawn-Davies/Makar/actions/workflows/ci.yml/badge.svg)](https://github.com/Arawn-Davies/Makar/actions/workflows/ci.yml)
[![Release](https://github.com/Arawn-Davies/Makar/actions/workflows/release.yml/badge.svg)](https://github.com/Arawn-Davies/Makar/actions/workflows/release.yml)

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
- VESA linear framebuffer + bitmap-font text renderer (pane API for split-screen)
- Physical memory manager (bitmap allocator)
- Paging (256 MiB identity map, 4 MiB large pages) + per-task VMM (4 KiB pages)
- Kernel heap (`kmalloc` / `kfree` / `krealloc`)
- PIT timer (50 Hz) + `ksleep`
- INT 1 / INT 3 debug handlers (GDB-friendly)
- PS/2 keyboard driver (IRQ 1, scan-code set 1, ring buffer, arrow keys, Ctrl codes)
- ATA/IDE PIO driver (28-bit LBA, polling, read + write, 4 drives, SRST on init)
- MBR partition table — read + interactive creation (`mkpart mbr`), MDFS type `0xFA`
- GPT partition table — read + interactive creation (`mkpart gpt`) with CRC32 headers
- FAT32 filesystem driver — read + write, auto-mounted at `/hd` on boot
- ISO 9660 filesystem — read-only, auto-mounted at `/cdrom` when CD-ROM present
- VFS layer — unified path routing, `ls`, `cd`, `cat`, `cp`, `mkdir`, `rm`
- Cooperative round-robin task scheduler (`task_yield`, `task_exit`, fixed pool)
- `int 0x80` syscall interface (Linux i386 ABI: `SYS_EXIT`, `SYS_WRITE`, `SYS_READ`, `SYS_YIELD`, `SYS_DEBUG`)
- Ring-3 userspace: ELF loader, VMM page directories, `ring3_enter` via `iret`
- Userspace apps: `hello` (hello world), `calc` (bc-style expression evaluator)
- Installed HDD boot: `makar-hdd.img` — MBR + FAT32 + GRUB 2, self-contained
- Kernel shell: `help`, `clear`, `echo`, `meminfo`, `uptime`, `shutdown`, `lsdisks`,
  `lspart`, `mkpart`, `readsector`, `ls`, `cd`, `cat`, `cp`, `mkdir`, `rm`,
  `mount`, `exec`, `ktest`, `ring3test`, `vicstest`
- VICS — in-kernel text editor (VGA + VESA)

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
