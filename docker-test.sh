#!/bin/bash
# docker-test.sh – build a debug ISO via Docker then run the full test suite
# on host QEMU and host GDB.
#
# Usage: ./docker-test.sh
#
# Requirements (host):
#   • docker
#   • qemu-system-i386  (or the appropriate qemu-system-<arch>)
#   • GDB with Python support: i686-elf-gdb, gdb-multiarch, or gdb
#
# The build step runs inside arawn780/gcc-cross-i686-elf:fast, so no local
# cross-compiler is needed.  Running and testing still happens on the host
# because QEMU requires direct hardware / display access that is unavailable
# inside a container.

set -e

. ./src/config.sh
QEMU_BIN="qemu-system-$(./src/target-triplet-to-arch.sh "$HOST")"

# ── Pre-flight checks ────────────────────────────────────────────────────────
if ! command -v "$QEMU_BIN" >/dev/null 2>&1; then
    echo "ERROR: host QEMU not found (expected '$QEMU_BIN')." >&2
    exit 1
fi

if   command -v i686-elf-gdb   >/dev/null 2>&1; then GDB=i686-elf-gdb
elif command -v gdb-multiarch   >/dev/null 2>&1; then GDB=gdb-multiarch
elif command -v gdb             >/dev/null 2>&1; then GDB=gdb
else
    echo "ERROR: no suitable GDB found (tried i686-elf-gdb, gdb-multiarch, gdb)." >&2
    exit 1
fi

echo "==> Using GDB: $GDB"

if ! "$GDB" --batch -ex "python print('python-ok')" /dev/null 2>/dev/null \
        | grep -q 'python-ok'; then
    echo "ERROR: $GDB does not have Python scripting support." >&2
    echo "       Rebuild GDB with --with-python, or install gdb-multiarch." >&2
    exit 1
fi

# ── Docker build (debug flags for accurate GDB symbols) ──────────────────────
export CFLAGS='-O0 -g3'
echo "==> Building debug ISO in Docker (CFLAGS='$CFLAGS')..."
./docker-iso.sh

# ── Serial smoke test ────────────────────────────────────────────────────────
echo "==> Running serial smoke test..."
timeout 20 "$QEMU_BIN" \
    -cdrom makar.iso \
    -serial stdio \
    -display none \
    -no-reboot \
    -no-shutdown \
    -d int,cpu_reset \
    -D qemu-debug.log \
    2>&1 | tee serial.log || true

grep -q "serial: COM1 ready" serial.log
grep -q "keyboard: PS/2 IRQ1 handler registered" serial.log
echo "==> Serial smoke test passed."

# ── GDB boot test suite ──────────────────────────────────────────────────────
echo "==> Starting QEMU (GDB stub on :1234)..."
"$QEMU_BIN" \
    -cdrom makar.iso \
    -serial stdio \
    -display none \
    -no-reboot \
    -no-shutdown \
    -s -S &
QEMU_PID=$!

# Give QEMU a moment to open the GDB stub socket.
sleep 2

echo "==> Running GDB test suite..."
timeout 60 "$GDB" -batch \
    -ex "source tests/gdb_boot_test.py" \
    src/kernel/makar.kernel \
    2>&1 | tee gdb-test.log
GDB_EXIT=${PIPESTATUS[0]}

echo "==> Stopping QEMU..."
kill "$QEMU_PID" 2>/dev/null || true
wait "$QEMU_PID" 2>/dev/null || true

if [ "$GDB_EXIT" -eq 0 ]; then
    echo "==> All GDB tests PASSED."
else
    echo "==> GDB tests FAILED (exit $GDB_EXIT). See gdb-test.log for details."
fi

exit "$GDB_EXIT"
