# Testing Makar

This guide covers the automated test infrastructure. For build and run
instructions see [Building & Running](building.md).

---

## Serial smoke test

The quickest way to verify a build boots correctly is the serial smoke test.
It checks for two expected lines on the serial port:

```
serial: COM1 ready
keyboard: PS/2 IRQ1 handler registered
```

### Running the smoke test

**Docker Compose** (recommended — no host QEMU required):

```sh
docker compose run --rm test
```

**docker-test.sh** (builds in Docker, tests on host QEMU):

```sh
bash docker-test.sh
```

**test-gdb.sh** (native — builds and tests locally, no Docker):

```sh
bash test-gdb.sh
```

### How it works

QEMU runs headless (`-display none`) with serial on `stdio`, and output is
captured to `serial.log`.  If either expected line is missing, the test fails.

---

## GDB boot-test suite

`test-gdb.sh` (native) and `docker-test.sh` (Docker) both run the Python-based
GDB test suite located at `tests/gdb_boot_test.py`:

1. QEMU starts frozen (`-s -S`) with the GDB stub on `:1234`.
2. GDB connects, sources the test script, and runs assertions against kernel
   state (symbol addresses, memory layout, boot progress).
3. Results are written to `gdb-test.log`.

GDB must have Python scripting support.  The scripts auto-detect the best
available GDB binary in this order: `i686-elf-gdb` → `gdb-multiarch` → `gdb`.

### Prerequisites

| Tool | Needed by |
|---|---|
| `qemu-system-i386` | All test methods |
| GDB with Python support | `test-gdb.sh`, `docker-test.sh` |
| Docker | `docker-test.sh`, `docker compose run --rm test` |

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

The `-O0 -g3` flags ensure DWARF debug info is accurate.  `gdb.sh` freezes
the CPU at reset (`-S`) so you have time to set breakpoints before the kernel
begins executing.

---

## CI

The GitHub Actions workflow (`.github/workflows/build.yml`) runs the same
`test` Compose service that you run locally, so anything that passes
`docker compose run --rm test` locally will pass in CI.
