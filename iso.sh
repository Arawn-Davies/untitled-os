#!/bin/sh
set -e
. ./build.sh

mkdir -p isodir
mkdir -p isodir/boot
mkdir -p isodir/boot/grub

cp sysroot/boot/makar.kernel isodir/boot/makar.kernel
cat > isodir/boot/grub/grub.cfg << EOF
set default=0
set timeout=0

menuentry "Makar" {
	multiboot2 /boot/makar.kernel
}
EOF
grub-mkrescue -o makar.iso isodir
