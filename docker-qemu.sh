#!/bin/sh
set -e
. ./src/config.sh
QEMU_BIN="qemu-system-$(./src/target-triplet-to-arch.sh "$HOST")"

# Build with the CI Docker image, then run locally with host QEMU.
./docker-iso.sh

# Create a blank hard-disk image if one does not already exist.
if [ ! -f hdd.img ]; then
    ./mkhdd.sh
fi

if ! command -v "$QEMU_BIN" >/dev/null 2>&1; then
    echo "ERROR: host QEMU not found (expected '$QEMU_BIN')." >&2
    exit 1
fi

"$QEMU_BIN" \
    -drive file=hdd.img,format=raw,if=ide,index=0 \
    -drive file=makar.iso,if=ide,index=2,media=cdrom \
    -boot order=d \
    -serial stdio
