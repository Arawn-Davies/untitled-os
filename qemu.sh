#!/bin/sh
set -e
. ./iso.sh

# Create a blank hard-disk image if one does not already exist.
# Run ./mkhdd.sh to wipe and re-create it from scratch.
if [ ! -f hdd.qcow2 ]; then
    ./mkhdd.sh
fi

# Boot order 'd' means CD-ROM first; the HDD has no MBR yet so booting
# from it would fail.  The HDD is attached as the primary slave so the
# kernel's ATA driver can enumerate it.
qemu-system-$(./src/target-triplet-to-arch.sh $HOST) \
    -cdrom makar.iso \
    -drive file=hdd.qcow2,format=qcow2,if=ide,index=1 \
    -boot order=d \
    -serial stdio
./clean.sh
