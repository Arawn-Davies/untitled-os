#!/bin/sh
set -e
. ./build.sh

mkdir -p isodir
mkdir -p isodir/boot
mkdir -p isodir/boot/grub

cp sysroot/boot/untitled-os.kernel isodir/boot/untitled-os.kernel
cat > isodir/boot/grub/grub.cfg << EOF
set default=0
set timeout=0

menuentry "AOYU OS" {
	multiboot2 /boot/untitled-os.kernel
}
EOF
grub-mkrescue -o untitled-os.iso isodir
