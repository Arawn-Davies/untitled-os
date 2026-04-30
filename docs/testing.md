# Testing Makar

This guide covers the automated test infrastructure. For build and run
instructions see [Building & Running](building.md).

---

## CI test suite (`docker-ktest.sh`)

The single command for complete CI validation:

```sh
./docker-ktest.sh
```

Runs two steps, both fully inside Docker (no host toolchain needed):

**Step 1 — in-kernel ktest suite**

Builds a `TEST_MODE` ISO. The kernel boots, runs `ktest_run_all()` (all
subsystem unit tests including a live ring-3 userspace execution), then exits
QEMU cleanly via `isa-debug-exit`. Output: `ktest.log`.

**Step 2 — GDB boot-checkpoint tests**

Builds a normal debug ISO. Launches QEMU with the GDB stub, then runs
`tests/gdb_boot_test.py` to verify Multiboot 2 magic, symbol addresses, and
every major boot checkpoint from outside the kernel. Output: `gdb-test.log`.

Exit code 0 = everything passed; 1 = any failure or timeout.

---

---

## HDD boot test (`docker-hdd-test.sh`)

Verifies the installed HDD boot path end-to-end — no CD-ROM attached:

```sh
./docker-hdd-test.sh
```

What it does:

1. **Clean rebuild** — ensures `src/kernel/makar.kernel` (GDB symbol file) matches the binary written into the image.
2. **Generate `makar-hdd-test.img`** — a fresh raw MBR + FAT32 HDD image with GRUB 2.  Uses the interactive (non-TEST_MODE) kernel so `shell_run` is called and `vfs_auto_mount()` runs.  Kept separate from `makar-hdd.img` so interactive and test images never share state.
3. **GDB boot test** — boots the image inside Docker with `-boot c` (HDD-only) and runs `tests/gdb_hdd_test.py`.

Test groups in `gdb_hdd_test.py`:

| Group | What it verifies |
|---|---|
| `boot_checkpoints` | Every major boot function is reached (`kernel_main` → `shell_run`) |
| `hardware_state` | CR0.PG set, CR3 non-zero, PIT is ticking |
| `hdd_mount` | `fat32_mounted()` returns non-zero — FAT32 partition auto-mounted at `/hd` |

The `hdd_mount` check continues execution to `keyboard_getchar` (the shell's read loop entry) before inspecting `fat32_mounted()`, ensuring `vfs_auto_mount()` has fully completed.

Output files: `hdd-test-gdb.log`, `hdd-test-serial.log`.

---

## Debugging with GDB

```sh
# Terminal 1 — start QEMU with GDB stub
bash gdb.sh

# Terminal 2 — attach GDB
i686-elf-gdb src/kernel/makar.kernel \
    -ex "target remote :1234" \
    -ex "break kernel_main" \
    -ex "continue"
```

The `-O0 -g3` flags ensure DWARF debug info is accurate. `gdb.sh` freezes
the CPU at reset (`-S`) so you have time to set breakpoints before the kernel
begins executing.

---

## Prerequisites

| Tool | Needed by |
|---|---|
| Docker | `docker-ktest.sh` (all steps run inside the container) |
| `qemu-system-i386` (host) | `docker-qemu.sh` only |

---

## CI

The GitHub Actions workflow (`.github/workflows/build.yml`) runs
`docker-ktest.sh`, so anything that passes locally will pass in CI.
