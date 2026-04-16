#!/bin/sh
set -e
. ./src/config.sh

# Build with the CI Docker image, then run locally with host QEMU.
./docker-iso.sh

# Create a blank hard-disk image if one does not already exist.
if [ ! -f hdd.img ]; then
    ./mkhdd.sh
fi

qemu-system-$(./src/target-triplet-to-arch.sh "$HOST") \
    -drive file=hdd.img,format=raw,if=ide,index=0 \
    -drive file=makar.iso,if=ide,index=2,media=cdrom \
    -boot order=d \
    -serial stdio
