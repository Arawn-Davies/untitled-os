# Makar

[![Build](https://github.com/Arawn-Davies/Makar/actions/workflows/build.yml/badge.svg)](https://github.com/Arawn-Davies/Makar/actions/workflows/build.yml)

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
- ATA/IDE PIO driver (28-bit LBA, polling, read + write, 4 drives)
- MBR partition table — read (`lspart`) + interactive creation (`mkpart mbr`),
  MDFS type `0xFA` supported
- GPT partition table — read + interactive creation (`mkpart gpt`) with
  CRC32-signed headers, auto-generated UUIDs, and known type GUIDs
  (FAT32 / EFI / Linux / MDFS)
- Kernel shell (`help`, `clear`, `echo`, `meminfo`, `uptime`, `shutdown`,
  `lsdisks`, `lspart`, `mkpart`, `readsector`)

## Documentation

See [`docs/`](docs/index.md) for full subsystem documentation and the
[Makar × Medli](docs/makar-medli.md) co-operation roadmap.

## Build

### Native (cross-compiler on your machine)

```sh
bash iso.sh        # build makar.iso
bash qemu.sh       # build and run in QEMU
bash gdb.sh        # build and launch with GDB stub on :1234
```

Requires: `i686-elf-gcc`, `nasm`, `grub-mkrescue`, `xorriso`, `qemu-system-i386`

### Docker helper scripts

Build inside the CI Docker image, then (optionally) run/test with host QEMU:

```sh
bash docker-iso.sh  # build makar.iso using the CI Docker image
bash docker-test.sh # build in Docker, run headless host-QEMU smoke test + GDB test suite
bash docker-qemu.sh # build in Docker, run interactively with host QEMU
```

Requires: `docker` and (for `docker-test.sh` / `docker-qemu.sh`) host
`qemu-system-i386`.  No cross-compiler needed — the container provides it.

### Docker Compose

The repository includes a `Dockerfile` and `docker-compose.yml` that wrap the
same CI image (`arawn780/gcc-cross-i686-elf:fast`) for convenient local
development.

| Service | What it does |
|---|---|
| `build` | Compiles the kernel and produces `makar.iso` with release flags (`-O2 -g`). |
| `build-debug` | Same as `build` but compiles with `-O0 -g3` for accurate GDB symbols. |
| `test` | Builds a debug ISO then runs the serial smoke test inside the container using headless QEMU — no host QEMU required. |

```sh
docker compose run --rm build          # produce makar.iso
docker compose run --rm build-debug    # produce a debug makar.iso
docker compose run --rm test           # build + headless serial smoke test
```

The source tree is bind-mounted at `/work`, so build output (`makar.iso`,
`sysroot/`, `isodir/`) lands directly in your checkout.

> **Tip:** You can also build and use the image standalone:
> ```sh
> docker build -t makar .
> docker run --rm -v "$PWD":/work -w /work makar   # runs bash iso.sh
> ```
