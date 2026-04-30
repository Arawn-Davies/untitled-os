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
| `docker-hdd-boot.sh` | Cleans, builds an interactive kernel, generates `makar-hdd.img`, then launches QEMU on the host booting from the HDD (`-boot c`, no CD-ROM). | Docker, `qemu-system-i386` |
| `docker-hdd-test.sh` | Cleans, builds, generates `makar-hdd-test.img`, then runs the GDB HDD boot test fully inside Docker. Outputs `hdd-test-gdb.log` and `hdd-test-serial.log`. | Docker |
| `generate-hdd.sh` | Low-level: creates a raw MBR + FAT32 HDD image with GRUB 2 in the embedding area. Called by the two scripts above; can also be run directly. Accepts `--test` to write `test` kernel arg in `grub.cfg`. | Docker |

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
| `DOCKER_IMAGE` | `arawn780/gcc-cross-i686-elf:fast` | Docker image used for ISO build and test steps. |
| `BUILD_IMAGE` | `arawn780/gcc-cross-i686-elf:fast` | Docker image used by `generate-hdd.sh` for the disk-creation step. |
| `HDD_IMG` | `makar-hdd.img` | Output filename for `generate-hdd.sh` / `docker-hdd-boot.sh`. |
| `HDD_TEST_IMG` | `makar-hdd-test.img` | Output filename used by `docker-hdd-test.sh` (kept separate from the interactive image). |
| `HDD_SIZE_MB` | `512` | Size of the HDD image in MiB. |

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

The `arawn780/gcc-cross-i686-elf:fast` image ships everything needed for building, testing, and HDD image creation:

| Tool / package | Purpose |
|---|---|
| `i686-elf-gcc` / `i686-elf-binutils` | Bare-metal cross-compiler (GCC 13.2, Binutils 2.41) |
| `grub-mkimage`, `grub-file`, `grub-pc-bin` | ISO and HDD GRUB image creation; i386-pc modules + `boot.img` |
| `xorriso`, `mtools` | `grub-mkrescue` ISO packaging |
| `dosfstools` | `mkfs.fat` for FAT32 partition creation in HDD images |
| `fdisk` | `sfdisk` for MBR partition table writing in HDD images |
| `qemu-system-i386` | Headless boot testing inside Docker |
| `gdb-multiarch` | GDB with i386 target for boot-checkpoint tests |
| `make`, `build-essential`, `nasm` | Host build tools |

The Dockerfile that produced this image is `Dockerfile.compiler` in the repo root.
To rebuild and push after adding packages: `docker buildx build --platform linux/amd64 -t arawn780/gcc-cross-i686-elf:fast -f Dockerfile.compiler --push .`

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
