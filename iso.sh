#!/bin/sh
set -e
. ./build.sh

mkdir -p isodir/boot/grub/i386-pc

cp sysroot/boot/makar.kernel isodir/boot/makar.kernel
cat > isodir/boot/grub/grub.cfg << EOF
set default=0
set timeout=0

menuentry "Makar" {
	multiboot2 /boot/makar.kernel
}
EOF

# Include boot.img so the installer can read it from the ISO9660 filesystem.
# grub-mkrescue embeds it in the boot sectors but does not place it in the
# directory tree; we do that explicitly here.
for _d in /usr/lib/grub/i386-pc /usr/share/grub/i386-pc; do
    if [ -f "$_d/boot.img" ]; then
        cp "$_d/boot.img" isodir/boot/grub/i386-pc/boot.img
        break
    fi
done

grub-mkrescue -o makar.iso isodir
