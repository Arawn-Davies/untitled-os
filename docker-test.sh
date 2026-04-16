#!/bin/sh
set -e
. ./src/config.sh
QEMU_BIN="qemu-system-$(./src/target-triplet-to-arch.sh "$HOST")"

# Build with the CI Docker image, then run headless smoke tests on host QEMU.
./docker-iso.sh

if ! command -v "$QEMU_BIN" >/dev/null 2>&1; then
    echo "ERROR: host QEMU not found (expected '$QEMU_BIN')." >&2
    exit 1
fi

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

echo "==> Host-QEMU smoke test passed."
