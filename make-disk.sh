#!/bin/sh
# make-disk.sh — create a 512 MiB raw disk image for use with QEMU.
#
# Usage: . ./make-disk.sh        (source it so $DISK_IMAGE is exported)
#        ./make-disk.sh          (standalone: just creates the image)
#
# If the image already exists it is left untouched and reused as-is,
# so existing partition tables / filesystem data survive across runs.
#
# The variable DISK_IMAGE is set to the path of the image so that
# callers can embed it directly in a QEMU command line, e.g.:
#
#   -drive file="$DISK_IMAGE",format=raw,media=disk,if=ide,index=0

DISK_IMAGE="${DISK_IMAGE:-makar-disk.img}"
DISK_SIZE="${DISK_SIZE:-512M}"

if [ ! -f "$DISK_IMAGE" ]; then
    echo "==> Creating ${DISK_SIZE} disk image: ${DISK_IMAGE}"
    qemu-img create -f raw "$DISK_IMAGE" "$DISK_SIZE"
else
    echo "==> Reusing existing disk image: ${DISK_IMAGE}"
fi

export DISK_IMAGE
