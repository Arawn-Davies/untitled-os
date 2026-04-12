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

# Locate the host GRUB i386-pc directory.
GRUB_DIR=""
for _d in /usr/lib/grub/i386-pc /usr/share/grub/i386-pc; do
    if [ -d "$_d" ]; then
        GRUB_DIR="$_d"
        break
    fi
done

if [ -z "$GRUB_DIR" ]; then
    echo "Warning: GRUB i386-pc directory not found." >&2
    echo "The 'install' command will fail to read GRUB files from /boot/grub/i386-pc/." >&2
else
    # boot.img: embedded in the MBR area by grub-mkrescue but also needed
    # as a plain ISO9660 file so the installer can read and patch it.
    if [ -f "$GRUB_DIR/boot.img" ]; then
        cp "$GRUB_DIR/boot.img" isodir/boot/grub/i386-pc/boot.img
    else
        echo "Warning: boot.img not found in $GRUB_DIR." >&2
    fi

    # core.img: generate a standalone BIOS-bootable core image that the
    # installer will write to sectors 1..N of the target HDD.
    # The prefix (/boot/grub) tells GRUB where to look for modules once it
    # has been loaded from the HDD's FAT32 partition.
    #
    # The embedded early config (-c) runs a 'search' before normal starts so
    # that GRUB probes all partitions and sets root to whichever one contains
    # /boot/grub/grub.cfg.  Without this, GRUB defaults to the raw disk
    # device (hd0) instead of the FAT32 partition (hd0,msdos1) and never
    # finds grub.cfg, dropping straight to the command prompt.
    _embed_cfg=$(mktemp)
    printf 'search --no-floppy --file --set=root /boot/grub/grub.cfg\n' \
        > "$_embed_cfg"
    grub-mkimage \
        -O i386-pc \
        -o isodir/boot/grub/i386-pc/core.img \
        -p /boot/grub \
        -c "$_embed_cfg" \
        biosdisk part_msdos fat search search_fs_file normal multiboot2 linux \
        || echo "Warning: grub-mkimage failed; core.img will be missing." >&2
    rm -f "$_embed_cfg"

    # GRUB modules copied to the ISO so the installer can transfer them to
    # the FAT32 partition.  Missing modules are non-fatal (skipped silently).
    for _mod in normal part_msdos fat multiboot2 linux; do
        if [ -f "$GRUB_DIR/${_mod}.mod" ]; then
            cp "$GRUB_DIR/${_mod}.mod" "isodir/boot/grub/i386-pc/${_mod}.mod"
        fi
    done
fi

grub-mkrescue -o makar.iso isodir
