#!/bin/sh
set -e
. ./iso.sh
. ./make-disk.sh

qemu-system-$(./src/target-triplet-to-arch.sh $HOST) \
    -cdrom makar.iso \
    -drive file="$DISK_IMAGE",format=raw,media=disk,if=ide,index=0 \
    -serial stdio
./clean.sh
