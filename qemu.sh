#!/bin/sh
set -e
. ./iso.sh

# Create a blank hard-disk image if one does not already exist.
# Run ./mkhdd.sh to wipe and re-create it from scratch.
if [ ! -f hdd.img ]; then
    ./mkhdd.sh
fi

# Drive layout:
#   index=0 (primary master,   kernel drive 0): HDD — reliably detected by
#            the ATA PIO driver because it sits in the first IDE slot.
#   index=2 (secondary master, kernel drive 2): CD-ROM — QEMU's default
#            slot for -cdrom media.
#
# Boot order 'd' boots from the CD-ROM first.  The HDD is present so the
# installer (and the kernel's auto-mount) can access it at drive 0.
qemu-system-$(./src/target-triplet-to-arch.sh $HOST) \
    -drive file=hdd.img,format=raw,if=ide,index=0 \
    -drive file=makar.iso,if=ide,index=2,media=cdrom \
    -boot order=d \
    -serial stdio
./clean.sh
