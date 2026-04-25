# Building & Running Makar

The entire build and test toolchain runs inside Docker — no cross-compiler or
native GDB is required on the host.  For Windows-specific setup see
[WSL2 guide](wsl2.md).

---

## Prerequisites

| Tool | Purpose |
|---|---|
| `docker` | Runs the CI image that contains the full cross-toolchain |
| `qemu-system-i386` (host) | Required only for `docker-qemu.sh` — the container builds, the host runs QEMU |

---

## Quick start

```sh
# Interactive kernel shell (builds inside Docker, QEMU runs on host):
./docker-qemu.sh

# Full CI test suite (everything runs inside Docker, no host toolchain needed):
./docker-ktest.sh
```

---

## Scripts reference

### User-facing scripts

| Script | What it does | Host requirements |
|---|---|---|
| `docker-qemu.sh` | Cleans, builds an interactive debug ISO inside Docker, then launches QEMU on the host with the kernel shell. | Docker, `qemu-system-i386` |
| `docker-ktest.sh` | Full CI suite: step 1 = TEST_MODE ktest (in-kernel unit tests + ring-3 execution); step 2 = GDB boot-checkpoint verification. Everything runs inside Docker. | Docker |

### Internal build scripts (called inside Docker)

These scripts run inside the container via `docker-iso.sh` — you do not invoke
them directly.

| Script | What it does |
|---|---|
| `docker-iso.sh` | Entry point for Docker ISO builds; mounts the repo and calls `iso.sh`. |
| `build.sh` | Compiles the kernel and libc into `sysroot/`. |
| `iso.sh` | Calls `build.sh`, then packages the kernel into `makar.iso` via `grub-mkrescue`. |
| `clean.sh` | Removes `sysroot/`, `isodir/`, and all build artifacts. |

### Environment variables

| Variable | Default | Effect |
|---|---|---|
| `CFLAGS` | `-O2 -g` | Compiler flags. `docker-ktest.sh` sets `-O0 -g3` automatically. |
| `CPPFLAGS` | *(empty)* | Preprocessor flags. `docker-ktest.sh` sets `-DTEST_MODE` for step 1. |
| `DOCKER_BIN` | `docker` | Path or name of the Docker CLI binary. |
| `DOCKER_IMAGE` | `arawn780/gcc-cross-i686-elf:fast` | Docker image used for all builds. |

---

## Docker Compose

The `docker-compose.yml` defines services that wrap the same CI image.

| Service | Flags | Command inside container |
|---|---|---|
| `build` | *(default)* | `bash iso.sh` |
| `build-debug` | `CFLAGS="-O0 -g3"` | `bash iso.sh` |
| `test` | `CFLAGS="-O0 -g3"` | `bash iso.sh && qemu-system-i386 … -display none` |

```sh
docker compose run --rm build          # release ISO
docker compose run --rm build-debug    # debug ISO
docker compose run --rm test           # build + headless boot test
```

---

## What the Docker image provides

The `arawn780/gcc-cross-i686-elf:fast` image ships:

- `i686-elf-gcc` / `i686-elf-binutils` cross-toolchain
- `grub-mkrescue`, `grub-mkimage`, `grub-file`, `xorriso`
- `qemu-system-i386`
- `gdb-multiarch`, `make`

---

## QEMU drive layout

`docker-qemu.sh` launches QEMU with two IDE drives:

| IDE slot | QEMU flag | Purpose |
|---|---|---|
| Primary master (index 0) | `-drive file=hdd.img,…,index=0` | Hard disk (512 MiB raw image, auto-created) |
| Secondary master (index 2) | `-drive file=makar.iso,…,media=cdrom` | Live CD — GRUB boots from here |

Boot order is `d` (CD-ROM first).

---

## Testing & debugging

See [Testing](testing.md) for the ktest suite, GDB boot-checkpoint tests,
and interactive GDB debugging.
