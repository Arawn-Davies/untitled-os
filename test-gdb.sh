#!/bin/bash
# test-gdb.sh – build a debug ISO and run the GDB boot-test suite locally.
#
# Usage: ./test-gdb.sh
#
# Requirements: the same tools as the CI job (qemu-system-i386, gdb-multiarch,
# grub-mkrescue, xorriso, the i686-elf cross-compiler).
#
# The script mirrors the CI workflow in .github/workflows/build.yml so that
# whatever passes here is guaranteed to pass in CI.

set -e

. ./config.sh

# Build with full debug info and no optimisation so GDB gets accurate symbols,
# matching what gdb.sh does.
export CFLAGS='-O0 -g3'

echo "==> Building debug ISO..."
. ./iso.sh

echo "==> Starting QEMU (GDB stub on :1234)..."
qemu-system-$(./target-triplet-to-arch.sh "$HOST") \
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
timeout 60 gdb-multiarch -batch \
    -ex "source tests/gdb_boot_test.py" \
    kernel/makar.kernel \
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
