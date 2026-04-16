#!/bin/sh
set -e

# Build with the CI Docker image, then run headless smoke tests on host QEMU.
./docker-iso.sh

timeout 20 qemu-system-i386 \
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
