#!/bin/sh
set -e
. ./config.sh

# Build with full debug info and no optimisation so GDB gets accurate symbols
export CFLAGS='-O0 -g3'

. ./iso.sh

# -s  : shorthand for -gdb tcp::1234 (open GDB stub on port 1234)
# -S  : freeze CPU at startup and wait for GDB to connect
qemu-system-$(./target-triplet-to-arch.sh $HOST) \
    -cdrom untitled-os.iso \
    -serial stdio \
    -s -S
./clean.sh
