#!/bin/bash
# docker-test.sh – build a debug ISO and run the full test suite entirely
# inside the Docker build container.
#
# Usage: ./docker-test.sh
#
# Requirements (host):
#   • docker
#
# Both QEMU and GDB run inside arawn780/gcc-cross-i686-elf:fast so no local
# cross-compiler, QEMU, or GDB installation is needed.

set -e

REPO_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
DOCKER_IMAGE=${DOCKER_IMAGE:-arawn780/gcc-cross-i686-elf:fast}
DOCKER_BIN=${DOCKER_BIN:-docker}

# ── Docker build (debug flags for accurate GDB symbols) ──────────────────────
export CFLAGS='-O0 -g3'
echo "==> Building debug ISO in Docker (CFLAGS='$CFLAGS')..."
./docker-iso.sh

# ── Serial smoke test ────────────────────────────────────────────────────────
echo "==> Running serial smoke test..."
"$DOCKER_BIN" run --rm \
    -v "$REPO_ROOT:/work" \
    -w /work \
    "$DOCKER_IMAGE" \
    bash -c '
        timeout 20 qemu-system-i386 \
            -cdrom makar.iso \
            -serial stdio \
            -display none \
            -no-reboot \
            -no-shutdown \
            -d int,cpu_reset \
            -D /work/qemu-debug.log \
            2>/dev/null | tee /work/serial.log || true

        grep -q "serial: COM1 ready"                    /work/serial.log \
            || { echo "FAIL: COM1 ready not found in serial output"; exit 1; }
        grep -q "keyboard: PS/2 IRQ1 handler registered" /work/serial.log \
            || { echo "FAIL: keyboard init not found in serial output"; exit 1; }
        echo "==> Serial smoke test passed."
    '

# ── GDB boot test suite ──────────────────────────────────────────────────────
echo "==> Running GDB boot test suite..."
"$DOCKER_BIN" run --rm \
    -v "$REPO_ROOT:/work" \
    -w /work \
    "$DOCKER_IMAGE" \
    bash -c '
        qemu-system-i386 \
            -cdrom makar.iso \
            -serial file:/work/gdb-serial.log \
            -display none \
            -no-reboot \
            -no-shutdown \
            -s -S &
        QEMU_PID=$!

        # Wait for the GDB stub socket to open.
        sleep 2

        timeout 60 gdb-multiarch -batch \
            -ex "source tests/gdb_boot_test.py" \
            src/kernel/makar.kernel \
            2>&1 | tee /work/gdb-test.log
        GDB_EXIT=${PIPESTATUS[0]}

        kill "$QEMU_PID" 2>/dev/null || true
        wait "$QEMU_PID" 2>/dev/null || true

        exit "$GDB_EXIT"
    '
GDB_EXIT=$?

if [ "$GDB_EXIT" -eq 0 ]; then
    echo "==> All GDB tests PASSED."
else
    echo "==> GDB tests FAILED (exit $GDB_EXIT). See gdb-test.log for details."
fi

exit "$GDB_EXIT"
