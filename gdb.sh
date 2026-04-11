#!/bin/bash
# gdb.sh – build a debug kernel, start QEMU in the background, and drop
# straight into an interactive GDB session.
#
# QEMU runs headless with its GDB stub frozen at reset (-s -S).  Serial
# output is forwarded to a PTY so it doesn't interfere with the GDB UI.
# When GDB exits (whether the user quits or the connection drops), QEMU
# is shut down automatically.
#
# Usage:  ./gdb.sh

set -e

. ./src/config.sh

# Locate a suitable GDB (same preference order as test-gdb.sh).
if   command -v i686-elf-gdb  >/dev/null 2>&1; then GDB=i686-elf-gdb
elif command -v gdb-multiarch  >/dev/null 2>&1; then GDB=gdb-multiarch
elif command -v gdb            >/dev/null 2>&1; then GDB=gdb
else
    echo "ERROR: no suitable GDB found (tried i686-elf-gdb, gdb-multiarch, gdb)." >&2
    exit 1
fi

echo "==> Using GDB: $GDB"

# Build with full debug info and no optimisation so GDB gets accurate symbols.
export CFLAGS='-O0 -g3'

echo "==> Building debug ISO..."
. ./iso.sh

echo "==> Preparing disk image..."
. ./make-disk.sh

ARCH=$(./src/target-triplet-to-arch.sh "$HOST")

echo "==> Starting QEMU in background (serial → PTY, GDB stub on :1234)..."
qemu-system-"$ARCH" \
    -cdrom makar.iso \
    -drive file="$DISK_IMAGE",format=raw,media=disk,if=ide,index=0 \
    -serial pty \
    -display none \
    -no-reboot \
    -s -S &
QEMU_PID=$!

# Give QEMU a moment to open the GDB stub socket.
sleep 1

echo "==> Attaching GDB — type 'continue' (or 'c') to start the kernel."
echo "    QEMU will be stopped when you quit GDB."

# Run GDB interactively.  Pre-connect to the remote stub so the user lands
# straight at a (gdb) prompt with the kernel already loaded and ready.
"$GDB" \
    -ex "set architecture i386" \
    -ex "target remote :1234" \
    src/kernel/makar.kernel || true

echo "==> GDB exited — stopping QEMU..."
kill "$QEMU_PID" 2>/dev/null || true
wait "$QEMU_PID" 2>/dev/null || true
echo "==> Done."
