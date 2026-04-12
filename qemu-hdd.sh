#!/bin/sh
# qemu-hdd.sh — boot the installed Makar OS directly from the HDD image.
#
# Run this after using qemu.sh + the 'install' command to write Makar to
# hdd.img.  No CD-ROM is attached so the kernel boots from the HDD alone.
#
# Drive layout:
#   index=0 (primary master, kernel drive 0): HDD with GRUB + Makar
#
set -e

if [ ! -f hdd.img ]; then
    echo "hdd.img not found.  Run ./qemu.sh and use the 'install' command first." >&2
    exit 1
fi

qemu-system-$(./src/target-triplet-to-arch.sh $HOST) \
    -drive file=hdd.img,format=raw,if=ide,index=0 \
    -boot order=c \
    -serial stdio
