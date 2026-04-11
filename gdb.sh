#!/bin/sh
set -e
. ./src/config.sh

# Build with full debug info and no optimisation so GDB gets accurate symbols
export CFLAGS='-O0 -g3'

. ./iso.sh
. ./make-disk.sh

# -s  : shorthand for -gdb tcp::1234 (open GDB stub on port 1234)
# -S  : freeze CPU at startup and wait for GDB to connect
qemu-system-$(./src/target-triplet-to-arch.sh $HOST) \
    -cdrom makar.iso \
    -drive file="$DISK_IMAGE",format=raw,media=disk,if=ide,index=0 \
    -serial stdio \
    -s -S
./clean.sh
