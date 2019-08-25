#!/bin/sh
set -e
. ./build.sh

mkdir -p isodir
mkdir -p isodir/boot
mkdir -p isodir/boot/grub

cp sysroot/boot/untitled-os.kernel isodir/boot/untitled-os.kernel
cat > isodir/boot/grub/grub.cfg << EOF
menuentry "untitled-os" {
	multiboot /boot/untitled-os.kernel
}
EOF
grub-mkrescue -o untitled-os.iso isodir
