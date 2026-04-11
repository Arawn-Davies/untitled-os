#!/bin/sh
# mkhdd.sh — (re-)create the 512 MiB qcow2 hard-disk image used by QEMU.
#
# Run this script whenever you want a fresh blank disk, e.g.:
#   ./mkhdd.sh
#
# The image is excluded from version control (.gitignore).
set -e
qemu-img create -f qcow2 hdd.qcow2 512M
echo "hdd.qcow2 created (512 MiB, qcow2)."
