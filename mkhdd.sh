#!/bin/sh
# mkhdd.sh — (re-)create the 512 MiB raw disk image used by QEMU.
#
# A raw .img file can be mounted directly with OSFMount, losetup, etc.
#
# Run this script whenever you want a fresh blank disk, e.g.:
#   ./mkhdd.sh
#
# The image is excluded from version control (.gitignore).
set -e
qemu-img create -f raw hdd.img 512M
echo "hdd.img created (512 MiB, raw)."
