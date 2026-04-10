#!/bin/bash
# test-gdb.sh – build a debug ISO and run the GDB boot-test suite locally.
#
# Usage: ./test-gdb.sh
#
# Requirements: QEMU (qemu-system-i386), grub-mkrescue, xorriso, the
# i686-elf cross-compiler, and a GDB with Python support:
#   • i686-elf-gdb  (typical local cross-toolchain install)
#   • gdb-multiarch (Ubuntu/Debian package, used in CI)
#   • gdb           (last resort)
#
# The script mirrors the CI workflow in .github/workflows/build.yml so that
# whatever passes here is guaranteed to pass in CI.

set -e

. ./src/config.sh

# ── Locate a suitable GDB ────────────────────────────────────────────────────
# Prefer the same cross-toolchain GDB that matches the target (i686-elf-gdb),
# fall back to gdb-multiarch (the CI tool), then plain gdb.
if   command -v i686-elf-gdb  >/dev/null 2>&1; then GDB=i686-elf-gdb
elif command -v gdb-multiarch  >/dev/null 2>&1; then GDB=gdb-multiarch
elif command -v gdb            >/dev/null 2>&1; then GDB=gdb
else
    echo "ERROR: no suitable GDB found (tried i686-elf-gdb, gdb-multiarch, gdb)." >&2
    exit 1
fi

echo "==> Using GDB: $GDB"

# The test suite is written in Python and loaded via GDB's embedded interpreter.
# Verify Python support is compiled in before we start QEMU; if it isn't, GDB
# would silently parse the .py file as GDB commands and fail with cryptic errors.
if ! "$GDB" --batch -ex "python print('python-ok')" /dev/null 2>/dev/null \
        | grep -q 'python-ok'; then
    echo "ERROR: $GDB does not have Python scripting support." >&2
    echo "       Rebuild GDB with --with-python, or install gdb-multiarch." >&2
    exit 1
fi

# Build with full debug info and no optimisation so GDB gets accurate symbols,
# matching what gdb.sh does.
export CFLAGS='-O0 -g3'

echo "==> Building debug ISO..."
. ./iso.sh

echo "==> Starting QEMU (GDB stub on :1234)..."
qemu-system-$(./src/target-triplet-to-arch.sh "$HOST") \
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
