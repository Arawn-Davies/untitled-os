# Building & Running Makar

This guide covers every way to build, run, test, and debug Makar.  For the
Windows-specific WSL2 workflow see [WSL2 guide](wsl2.md).

---

## Prerequisites

### Native build (cross-compiler on your machine)

| Tool | Purpose |
|---|---|
| `i686-elf-gcc` / `i686-elf-ld` | Cross-compiler and linker ([Quick-i686](https://github.com/Arawn-Davies/quick-i686)) |
| `nasm` | Assembler for `.S` / `.asm` source files |
| `grub-mkrescue` | Creates the bootable ISO image |
| `xorriso` | Back-end used by `grub-mkrescue` |
| `qemu-system-i386` | Emulator for running / testing the ISO |
| `gdb` (with Python support) | Optional — required only for `gdb.sh` and `test-gdb.sh` |

### Docker build (no local cross-compiler needed)

| Tool | Purpose |
|---|---|
| `docker` | Runs the CI image that contains the full cross-toolchain |
| `qemu-system-i386` (host) | Needed only for `docker-qemu.sh` and `docker-test.sh` — the container builds, the host runs |
| `gdb` (host, with Python) | Needed only for `docker-test.sh` |

---

## Quick start

```sh
# Native — build and run in one step:
bash qemu.sh

# Docker — build inside the CI image, run with host QEMU:
bash docker-qemu.sh
```

---

## Scripts reference

### Build scripts

| Script | What it does | Requires |
|---|---|---|
| `build.sh` | Compiles the kernel and libc into `sysroot/`. Does **not** create an ISO. | Cross-compiler |
| `iso.sh` | Sources `build.sh`, then packages `sysroot/boot/makar.kernel` into `makar.iso` via `grub-mkrescue`. Also generates `core.img` and copies GRUB modules for the HDD installer. | Cross-compiler, `grub-mkrescue`, `xorriso` |
| `clean.sh` | Removes `sysroot/`, `isodir/`, and `makar.iso`. | — |

### Run scripts

| Script | What it does | Requires |
|---|---|---|
| `qemu.sh` | Builds the ISO (`iso.sh`), creates `hdd.img` if missing, then launches QEMU with both a CD-ROM and an IDE hard disk attached. Runs `clean.sh` on exit. | Cross-compiler, QEMU |
| `qemu-hdd.sh` | Boots directly from `hdd.img` (no CD-ROM). Use this after installing Makar to the HDD with the `install` shell command. | QEMU, existing `hdd.img` |
| `mkhdd.sh` | Creates (or re-creates) a blank 512 MiB raw disk image (`hdd.img`) using `qemu-img`. | `qemu-img` |

### Debug scripts

| Script | What it does | Requires |
|---|---|---|
| `gdb.sh` | Builds a debug ISO (`-O0 -g3`), launches QEMU frozen at startup with a GDB stub on `:1234` (`-s -S`). Connect with `target remote :1234` from GDB. | Cross-compiler, QEMU |
| `test-gdb.sh` | Local (non-Docker) variant of the full test suite. Builds a debug ISO, runs a serial smoke test, then executes the GDB boot-test suite (`tests/gdb_boot_test.py`). Mirrors CI. | Cross-compiler, QEMU, GDB with Python |

### Docker scripts

These build the ISO inside the `arawn780/gcc-cross-i686-elf:fast` Docker image
so you **do not need a local cross-compiler**.  Running and testing still use
the host QEMU (except for the Compose `test` service — see below).

| Script | What it does | Requires (host) |
|---|---|---|
| `docker-iso.sh` | Runs `iso.sh` inside the CI image (pulls the image on first use). Outputs `makar.iso` in the repo root via a bind mount. Forwards `CFLAGS` from the caller's environment. | Docker |
| `docker-qemu.sh` | Calls `docker-iso.sh`, creates `hdd.img` if missing, then launches QEMU on the host with HDD + CD-ROM. | Docker, QEMU |
| `docker-test.sh` | Calls `docker-iso.sh` with `-O0 -g3`, runs a serial smoke test on host QEMU, then runs the full GDB boot-test suite on host GDB. | Docker, QEMU, GDB with Python |

#### Environment variables

| Variable | Default | Effect |
|---|---|---|
| `CFLAGS` | *(empty — uses the Makefile default)* | Passed into the Docker build. Set to `-O0 -g3` for debug symbols. `docker-test.sh` sets this automatically. |
| `DOCKER_BIN` | `docker` | Path or name of the Docker CLI binary. |
| `DOCKER_IMAGE` | `arawn780/gcc-cross-i686-elf:fast` | Docker image used by `docker-iso.sh`. |

---

## Docker Compose

The `docker-compose.yml` defines three services that wrap the same CI image.
The source tree is bind-mounted at `/work` so all build output appears in your
checkout.

| Service | Flags | Command inside container |
|---|---|---|
| `build` | *(default)* | `bash iso.sh` |
| `build-debug` | `CFLAGS="-O0 -g3"` | `bash iso.sh` |
| `test` | `CFLAGS="-O0 -g3"` | `bash iso.sh && qemu-system-i386 … -display none` (headless smoke test) |

```sh
docker compose run --rm build          # release ISO
docker compose run --rm build-debug    # debug ISO
docker compose run --rm test           # build + headless serial smoke test
```

### Standalone image

You can also build and use the Docker image directly without Compose:

```sh
docker build -t makar .
docker run --rm -v "$PWD":/work -w /work makar            # runs bash iso.sh
docker run --rm -v "$PWD":/work -w /work makar bash gdb.sh  # or any script
```

### What the Dockerfile provides

The `Dockerfile` inherits `arawn780/gcc-cross-i686-elf:fast` and adds nothing
on top — the base image already ships:

- `i686-elf-gcc` / `i686-elf-binutils` cross-toolchain
- `grub-mkrescue`, `grub-mkimage`, `grub-file`, `xorriso`
- `qemu-system-i386`
- `gdb-multiarch`, `make`

The entire source tree is `COPY`-ed into the image so it can run standalone,
but when using `docker compose` or `docker run -v` the bind mount overlays
the baked-in copy.

---

## QEMU drive layout

`qemu.sh` and `docker-qemu.sh` launch QEMU with two IDE drives:

| IDE slot | QEMU flag | Kernel drive | Purpose |
|---|---|---|---|
| Primary master (index 0) | `-drive file=hdd.img,format=raw,if=ide,index=0` | Drive 0 | Hard disk — detected by the ATA PIO driver |
| Secondary master (index 2) | `-drive file=makar.iso,if=ide,index=2,media=cdrom` | Drive 2 | Live CD — GRUB boots from here |

Boot order is `d` (CD-ROM first).  After using the `install` shell command to
write Makar to the HDD you can boot from the HDD alone with `qemu-hdd.sh`.

---

## Testing & debugging

See [Testing](testing.md) for the serial smoke test, GDB boot-test suite,
and GDB debugging workflow.
